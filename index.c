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

typedef struct TreeNode {
    char is_dir;
    int nchilds;
    struct TreeNode **child;
    char *name;
} TreeNode;

TreeNode *new_treenode(const char *name) {
    TreeNode *t = malloc(sizeof(TreeNode));

    memset(t, 0, sizeof(*t));
    t->name = strdup(name);

    return t;
}

TreeNode *add_child(TreeNode *t, TreeNode *child) {
    t->child = realloc(t->child, (t->nchilds + 1) * sizeof(TreeNode*));
    t->child[t->nchilds++] = child;
}

int nresults;

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

    char *endpath = path + strlen(path);

    while((de = readdir(dp))) {
        if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        /* TODO: security */
        *endpath = '\0';
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
            strcat(path, "/");
            /* TODO: inotify_add_watch(de->d_name) */
            indexfs(child, path);
        }
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

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "usage: index ROOT\n");
        return 1;
    }

    char path[PATH_MAX > 32768 ? PATH_MAX : 32768];
    struct timeval start, stop;

    /* TODO: security */
    strcpy(path, argv[1]);

    TreeNode *t = new_treenode("");
    t->is_dir = 1; /* hopefully! */

    gettimeofday(&start, NULL);
    indexfs(t, path);
    gettimeofday(&stop, NULL);

    double secs = (stop.tv_sec - start.tv_sec) + (stop.tv_usec - start.tv_usec) / 1000000.0;

    printf("Indexing took %.3fs.\n", secs);

    if(isatty(STDOUT_FILENO)) {
        printf("? ");
        fflush(stdout);
    }

    char buf[1024];
    while(fgets(buf, 1024, stdin)) {
        buf[strlen(buf)-1] = '\0';
        *path = '\0';

        gettimeofday(&start, NULL);
        nresults = 0;
        search(t, path, buf);
        gettimeofday(&stop, NULL);
        double secs = (stop.tv_sec - start.tv_sec) + (stop.tv_usec - start.tv_usec) / 1000000.0;
        printf("%d results.\n", nresults);
        printf("Search took %.3fms.\n\n", secs * 1000.0);

        if(isatty(STDOUT_FILENO)) {
            printf("? ");
            fflush(stdout);
        }
    }

    return 0;
}
