/* Test of indexing the filesystem and searching that index */

#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/inotify.h>
#include <poll.h>
#include "uthash.h"

/* TODO:
 - if any tree operations become compute bound, start sorting t->child and
   binary searching
 - try storing children in a linked list instead of an array
*/

typedef struct TreeNode {
    char is_dir;
    int cookie;
    int wd;
    int nchilds;
    struct TreeNode *parent;
    struct TreeNode **child;
    char *name;
    UT_hash_handle hh;
} TreeNode;

int ifd = -1;
int nresults;
int ndirs, nfiles;
int *descr;

char path[PATH_MAX > 32768 ? PATH_MAX : 32768];

TreeNode *node_wd_hash;

TreeNode *new_treenode(const char *name) {
    TreeNode *t = malloc(sizeof(TreeNode));

    memset(t, 0, sizeof(*t));
    t->name = strdup(name);
    t->wd = -1;

    return t;
}

void add_child(TreeNode *t, TreeNode *child) {
    t->child = realloc(t->child, (t->nchilds + 1) * sizeof(TreeNode*));
    t->child[t->nchilds++] = child;
    child->parent = t;
}

TreeNode *lookup_node(TreeNode *t, char *path) {
    if(*path == '/')
        path++;

    while(*path && strcmp(path, "/") != 0) {
        char *endpath = NULL;
        if((endpath = strchr(path, '/')))
            *endpath = '\0';

        int i;
        for(i = 0; i < t->nchilds; i++) {
            if(strcmp(t->child[i]->name, path) == 0) {
                if(endpath) {
                    *endpath = '/';
                    path = endpath+1;
                } else {
                    path += strlen(path);
                }
                t = t->child[i];
                break;
            }
        }

        if(i == t->nchilds) /* no such child was found */
            return NULL;
    }

    return t;
}

void remove_node(TreeNode *t) {
    if(!(t->parent))
        return;

    int i;
    for(i = 0; i < t->parent->nchilds; i++) {
        if(t->parent->child[i] == t)
            break;
    }

    if(i == t->parent->nchilds) {
        /* !!! CONSISTENCY FAILURE !!! */
        fprintf(stderr, "warning: remove_node: node does not appear as a "
                "child of its parent!\n");
        return;
    }

    /* move the remaining children along */
    memmove(t->parent->child + i, t->parent->child + i + 1,
            sizeof(TreeNode*) * (t->parent->nchilds - i - 1));

    t->parent->nchilds--;

    t->parent->child = realloc(t->parent->child,
            t->parent->nchilds * sizeof(TreeNode*));
}

TreeNode *remove_path(TreeNode *t, char *path) {
    TreeNode *node = lookup_node(t, path);

    if(node)
        remove_node(node);

    return node;
}

char *node_name(TreeNode *t) {
    char *path = malloc(strlen(t->name) + 2);
    sprintf(path, "%s%s", t->name, t->is_dir ? "/" : "");

    t = t->parent;

    while(t) {
        if(*t->name) {
            char *newpath = malloc(strlen(t->name) + strlen(path) + 3);
            sprintf(newpath, "/%s/%s", t->name, path);
            free(path);
            path = newpath;
        }
        t = t->parent;
    }

    return path;
}

TreeNode *node_for_wd(int wd) {
    TreeNode *t;

    HASH_FIND_INT(node_wd_hash, &wd, t);

    return t;
}

void free_node(TreeNode *t) {
    int i;
    if(!t)
        return;
    for(i = 0; i < t->nchilds; i++) {
        free_node(t->child[i]);
    }
    if(t->is_dir && t->wd != -1)
        HASH_DEL(node_wd_hash, t);
    free(t->child);
    free(t->name);
    free(t);
}

/* recursively index the filesystem starting at the given path */
void indexfs(TreeNode *t, char *path) {
    DIR *dp;
    struct dirent *de;
    struct stat statbuf;

    if(strlen(path) > 16384) {
        fprintf(stderr, "fail: strlen(path) = %ld\n", strlen(path));
        exit(1);
    }

    if(!(dp = opendir(path))) {
        fprintf(stderr, "opendir %s: %s\n", path, strerror(errno));
        return;
    }

    int mask = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO;

    /* notice when the root node gets deleted */
    if(t->parent == NULL)
        mask |= IN_DELETE_SELF;

    /* TODO: watch IN_ATTRIB if access control is added */
    if((t->wd = inotify_add_watch(ifd, path, mask)) == -1) {
        fprintf(stderr, "inotify_add_watch(%s): %s\n", path, strerror(errno));
    } else {
        HASH_ADD_INT(node_wd_hash, wd, t);
    }

    char *endpath = path + strlen(path);

    strcat(path, "/");

    while((de = readdir(dp))) {
        if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        /* TODO: security */
        *(endpath+1) = '\0';
        strcat(path, de->d_name);

        TreeNode *child = new_treenode(de->d_name);
        add_child(t, child);

        if(lstat(path, &statbuf) == -1) {
            fprintf(stderr, "stat %s: %s\n", path, strerror(errno));
            continue;
        }

        child->is_dir = S_ISDIR(statbuf.st_mode) && !S_ISLNK(statbuf.st_mode);

        if(child->is_dir) {
            /* TODO: security */
            indexfs(child, path);
        }

        if(child->is_dir)
            ndirs++;
        else
            nfiles++;
    }

    closedir(dp);

    *endpath = '\0';
}

/* search the index, printing out any entries containing "term" */
void search(TreeNode *t, char *path, const char *term) {
    char *endpath = path + strlen(path);

    /* TODO: security */
    strcat(path, t->name);
    if(t->is_dir)
        strcat(path, "/");

    if(strstr(path, term)) {
        nresults++;
        printf("%s\n", path);
    }

    int i;
    for(i = 0; i < t->nchilds; i++) {
        search(t->child[i], path, term);
    }

    *endpath = '\0';
}

/* return -1 on error and 0 on success */
int do_inotify(TreeNode *t) {
    char buf[1024];
    struct inotify_event *ev[1024 / sizeof(struct inotify_event) + 1];
    static char *moved_from = NULL;

    int n;
    if((n = read(ifd, buf, 1024)) <= 0) {
        if(n < 0)
            perror("read");
        return -1;
    }

    int nevents = 0;
    int p = 0;
    while(p < n) {
        ev[nevents] = (struct inotify_event*)(buf + p);
        p += ev[nevents]->len + sizeof(struct inotify_event);
        nevents++;
    }

    int i;
    for(i = 0; i < nevents; i++) {
        TreeNode *t = node_for_wd(ev[i]->wd);

        char *name = NULL;
        if(t)
            name = node_name(t);

        printf("wd %d (%s):\n", ev[i]->wd, name);

        printf("  events: %08x =", ev[i]->mask);
        if(ev[i]->mask & IN_ACCESS)
            printf(" IN_ACCESS");
        if(ev[i]->mask & IN_MODIFY)
            printf(" IN_MODIFY");
        if(ev[i]->mask & IN_ATTRIB)
            printf(" IN_ATTRIB");
        if(ev[i]->mask & IN_OPEN)
            printf(" IN_OPEN");
        if(ev[i]->mask & IN_CLOSE_WRITE)
            printf(" IN_CLOSE_WRITE");
        if(ev[i]->mask & IN_CLOSE_NOWRITE)
            printf(" IN_CLOSE_NOWRITE");
        if(ev[i]->mask & IN_MOVED_FROM)
            printf(" IN_MOVED_FROM");
        if(ev[i]->mask & IN_MOVED_TO)
            printf(" IN_MOVED_TO");
        if(ev[i]->mask & IN_DELETE)
            printf(" IN_DELETE");
        if(ev[i]->mask & IN_CREATE)
            printf(" IN_CREATE");
        if(ev[i]->mask & IN_DELETE_SELF)
            printf(" IN_DELETE_SELF");
        if(ev[i]->mask & IN_ISDIR)
            printf(" IN_ISDIR");
        if(ev[i]->mask & IN_IGNORED)
            printf(" IN_IGNORED");
        printf("\n");

        printf("  cookie: %d\n", ev[i]->cookie);

        if(ev[i]->len > 0)
            printf("  name: %s\n", ev[i]->name);
        else
            printf("  name: (null)\n");

        if(ev[i]->mask & IN_CREATE) {
            TreeNode *new = new_treenode(ev[i]->name);
            char indexfrom[32768];
            fprintf(stderr, "{%s, %s, %s}\n", path, name, ev[i]->name);
            sprintf(indexfrom, "%s%s%s", path, name, ev[i]->name);
            fprintf(stderr, "{indexfrom=%s}\n", indexfrom);
            struct stat statbuf;
            if((lstat(indexfrom, &statbuf)) == 0) {
                add_child(t, new);
                if(S_ISDIR(statbuf.st_mode) && !S_ISLNK(statbuf.st_mode)) {
                    new->is_dir = 1;
                    strcat(indexfrom, "/");
                    indexfs(new, indexfrom);
                }
            } else {
                fprintf(stderr, "stat %s: %s\n", indexfrom, strerror(errno));
            }
        }
        if(ev[i]->mask & IN_DELETE_SELF) {
            fprintf(stderr, "error: deleted root node! bailing.\n");
            exit(1);
        }
        if(ev[i]->mask & IN_DELETE) {
            TreeNode *node = remove_path(t, ev[i]->name);
            free_node(node);
        }
        if(ev[i]->mask & IN_MOVED_TO) {
            moved_from = NULL;
            /* TODO: manipulate the tree, re-index from here */
        } else {
            if(moved_from) {
                fprintf(stderr, "error: have a moved_from, but didn't get a "
                        "moved_to!\n");
                exit(1);
            }
        }
        if(ev[i]->mask & IN_MOVED_FROM) {
            if(moved_from) {
                fprintf(stderr, "error: already have a moved_from!\n");
                exit(1);
            }
            moved_from = strdup(ev[i]->name);
        }

        free(name);
    }

    return 0;
}

/* return -1 on error or eof and 0 on success */
int do_search(TreeNode *t) {
    char buf[1024];
    char string[32768];
    struct timeval start, stop;

    if(fgets(buf, 1024, stdin) == NULL)
        return -1;
    buf[strlen(buf)-1] = '\0';

    nresults = 0;

    *string = '\0';

    gettimeofday(&start, NULL);
    search(t, string, buf);
    gettimeofday(&stop, NULL);

    double secs = (stop.tv_sec - start.tv_sec)
        + (stop.tv_usec - start.tv_usec) / 1000000.0;
    printf("%d results.\n", nresults);
    printf("Search took %.3fms.\n\n", secs * 1000.0);

    if(isatty(STDOUT_FILENO)) {
        printf("? ");
        fflush(stdout);
    }

    return 0;
}

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "usage: index ROOT\n");
        return 1;
    }

    struct timeval start, stop;

    if((ifd = inotify_init()) == -1) {
        perror("inotify_init");
        return 1;
    }

    /* TODO: security */
    strcpy(path, argv[1]);

    if(path[strlen(path)-1] == '/')
        path[strlen(path)-1] = '\0';

    TreeNode *t = new_treenode("");
    t->is_dir = 1; /* hopefully! */

    gettimeofday(&start, NULL);
    indexfs(t, path);
    gettimeofday(&stop, NULL);

    double secs = (stop.tv_sec - start.tv_sec) + (stop.tv_usec - start.tv_usec) / 1000000.0;

    printf("Indexing took %.3fs.\n", secs);
    printf("Indexed %d directories and %d files.\n", ndirs, nfiles);

    if(isatty(STDOUT_FILENO)) {
        printf("? ");
        fflush(stdout);
    }

    struct pollfd fds[2];

    while(1) {
        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;
        fds[1].fd = ifd;
        fds[1].events = POLLIN;

        if(poll(fds, 2, -1) == -1) {
            perror("poll");
            break;
        }

        if(fds[0].revents & POLLIN) {
            if(do_search(t) == -1)
                break;
        } else if(fds[0].revents) {
            fprintf(stderr, "warning: unexpected revents for stdin: %d\n",
                    fds[0].revents);
        }

        if(fds[1].revents & POLLIN) {
            if(do_inotify(t) == -1)
                break;
        } else if(fds[1].revents) {
            fprintf(stderr, "warning: unexpected revents for inotify: %d\n",
                    fds[1].revents);
        }
    }

    free_node(t);

    return 0;
}
