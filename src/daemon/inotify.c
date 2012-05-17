/* Handle inotify for jfindd
 *
 * James Stanley 2012
 */

#include "jfindd.h"

#define WATCH_MASK (IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO)

int inotify_fd;

/* structure for handler functions for inotify events */
struct MaskFunc {
    int mask;
    void (*func)(TreeNode *, TreeNode *, struct inotify_event *);
};

/* inotify handler functions */
void _inotify_create(TreeNode *root, TreeNode *parent,
        struct inotify_event *ev);
void _inotify_delete(TreeNode *root, TreeNode *parent,
        struct inotify_event *ev);
void _inotify_moved_from(TreeNode *root, TreeNode *parent,
        struct inotify_event *ev);
void _inotify_moved_to(TreeNode *root, TreeNode *parent,
        struct inotify_event *ev);

/* handler functions for each type of inotify event; masks can be ORd together
 * to call a function only when the mask matches
 */
static struct MaskFunc maskfunc[] = {
    { IN_CREATE,     _inotify_create },
    { IN_DELETE,     _inotify_delete },
    { IN_MOVED_FROM, _inotify_moved_from },
    { IN_MOVED_TO,   _inotify_moved_to },
    { 0,         0 }
};

/* initialise inotify, printing a message and dying if there is a problem */
void init_inotify(void) {
    if((inotify_fd = inotify_init()) == -1) {
        perror("inotify_init");
        exit(1);
    }
}

/* watch the given directory (corresponding to the given node) with inotify;
 * print an error and return as normal if watching fails
 */
void watch_directory(TreeNode *t, const char *path) {
    assert(t->dir);/* the node must be a directory */

    /* add the watch, and store it in the hash table if successful */
    if((t->dir->wd = inotify_add_watch(inotify_fd, path, WATCH_MASK)) == -1)
        fprintf(stderr, "inotify_add_watch: %s: %s\n", path, strerror(errno));
    else
        set_treenode_for_wd(t->dir->wd, t);
}

/* deal with any new inotify events */
void handle_inotify_events(TreeNode *root) {
    char buf[4096];

    /* TODO: poll with 0 timeout to check that there is actually data? */

    /* read from the inotify fd */
    int n;
    if((n = read(inotify_fd, buf, 1024)) <= 0) {
        if(n < 0)
            perror("inotify: read");
        else
            fprintf(stderr, "error: eof on inotify fd\n");
        exit(1);
    }

    /* handle each event */
    struct inotify_event *ev;
    int p = 0;
    while(p < n) {
        ev = (struct inotify_event*)(buf + p);
        p += ev->len + sizeof(struct inotify_event);

        /* lookup the node this wd describes */
        TreeNode *t = treenode_for_wd(ev->wd);

        /* don't do anything if we are being told to ignore a node we are
         * already ignoring
         */
        if(!t) {
            assert(ev->mask == IN_IGNORED);/* we can't handle unknown nodes */
            continue;
        }

        /* call the appropriate function for each mask */
        int called = 0;
        int i;
        for(i = 0; maskfunc[i].mask; i++) {
            if(ev->mask & maskfunc[i].mask) {
                maskfunc[i].func(root, t, ev);
                called = 1;
            }
        }

        /* complain if we didn't call any handler functions */
        if(!called) {
            fprintf(stderr, "error: received inotify event with unknown mask "
                    "0x%08x!\n", ev->mask);
            exit(1);
        }
    }

    /* reindex anything that has indexed=0 */
    reindex(root, root);
}

/* handle an IN_CREATE event */
void _inotify_create(TreeNode *root, TreeNode *parent,
        struct inotify_event *ev) {
    assert(ev->mask & IN_CREATE);/* has to be IN_CREATE */

    char *parentname = treenode_name(parent);

    TreeNode *new = new_treenode(ev->name);
    add_child(parent, new);

    char *newname = strallocat(parentname, ev->name, NULL);

    int dir;
    if((dir = isdir(newname, !new->complained)) == -1) {
        new->complained = 1;
        return;
    }

    /* no further work necessary if it is not a directory */
    if(!dir)
        new->indexed = 1;
}

/* handle an IN_DELETE event */
void _inotify_delete(TreeNode *root, TreeNode *parent,
        struct inotify_event *ev) {
    TreeNode *t = remove_path(parent, ev->name);

    assert(t);/* there should always be a node with the name */

    free_treenode(t);
}

/* handle an IN_MOVED_FROM event */
void _inotify_moved_from(TreeNode *root, TreeNode *parent,
        struct inotify_event *ev) {
    TreeNode *t = lookup_treenode(parent, ev->name);

    assert(t);/* there should always be a node with the name */

    set_node_moved_from(ev->cookie, t);
}

/* handle an IN_MOVED_TO event */
void _inotify_moved_to(TreeNode *root, TreeNode *parent,
        struct inotify_event *ev) {
    TreeNode *t = node_for_cookie(ev->cookie);

    assert(t);/* there should always be a node for the cookie */

    /* remove the node from its old place in the tree */
    remove_treenode(t);

    /* fix the filename */
    free(t->name);
    t->name = strdup(ev->name);

    /* insert the node under its new parent */
    add_child(parent, t);
}
