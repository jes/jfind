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
void _inotify_ignored(TreeNode *root, TreeNode *parent,
        struct inotify_event *ev);
void _print_inotify_event(struct inotify_event *ev);

/* handler functions for each type of inotify event; masks can be ORd together
 * to call a function only when the mask matches
 */
static struct MaskFunc maskfunc[] = {
    { IN_CREATE,     _inotify_create },
    { IN_DELETE,     _inotify_delete },
    { IN_MOVED_FROM, _inotify_moved_from },
    { IN_MOVED_TO,   _inotify_moved_to },
    { IN_IGNORED,    _inotify_ignored },
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

/* deal with any new inotify events
 * return 0 on success and -1 on failure
 */
int handle_inotify_events(TreeNode *root) {
#define INOTIFY_BUFSZ 4096
    char buf[INOTIFY_BUFSZ];

    /* TODO: poll with 0 timeout to check that there is actually data? */

    /* read from the inotify fd */
    int n;
    if((n = read(inotify_fd, buf, INOTIFY_BUFSZ)) <= 0) {
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

        /* output the event if in debug mode */
        if(debug_mode)
            _print_inotify_event(ev);

        /* report failure if the inotify event queue overflowed */
        if(ev->mask & IN_Q_OVERFLOW) {
            fprintf(stderr, "warning: inotify event queue overflow\n");
            return -1;
        }

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

    assert(p == n);/* we should use up *exactly* n bytes, no more */

    /* reindex anything that has indexed=0 */
    reindex(root, root);

    return 0;
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

    /* remove a node with the same name if there is one there already */
    free_treenode(remove_path(parent, ev->name));

    /* fix the filename */
    free(t->name);
    t->name = strdup(ev->name);

    /* insert the node under its new parent */
    add_child(parent, t);
}

/* handle an IN_IGNORED event */
void _inotify_ignored(TreeNode *root, TreeNode *parent,
        struct inotify_event *ev) {
    if(ev->wd != -1)
        remove_wd(ev->wd);

    /* since we won't be getting any more notifications for this node, mark it
     * as non-indexed; if it gets deleted pretty soon, all is great, otherwise
     * we will try to reindex and hopefully reinstall an inotify watcher; if
     * that fails, only then will we print an error message
     */
    parent->indexed = 0;
}

/* print the given inotify event in the form:
 * wd<tab>wdpath<tab>mask<tab>cookie<tab>name
 * where wdpath is the full path of the node for this wd, and mask is given
 * as a comma-separated list of the bits in ev->mask
 */
void _print_inotify_event(struct inotify_event *ev) {
    /* wd */
    printf("%d\t", ev->wd);

    /* wdpath */
    TreeNode *t = treenode_for_wd(ev->wd);
    char *path = NULL;
    if(t)
        path = treenode_name(t);
    printf("%s\t", path);
    if(path)
        free(path);

    /* mask */
    static const char *bitname[32] = {
        "IN_ACCESS", "IN_MODIFY", "IN_ATTRIB", "IN_CLOSE_WRITE",
        "IN_CLOSE_NOWRITE", "IN_OPEN", "IN_MOVED_FROM", "IN_MOVED_TO",
        "IN_CREATE", "IN_DELETE", "IN_DELETE_SELF", "IN_MOVE_SELF",
        "0x00001000", "IN_UNMOUNT", "IN_Q_OVERFLOW", "IN_IGNORED",
        "0x00010000", "0x00020000", "0x00040000", "0x00080000", "0x00100000",
        "0x00200000", "0x00400000", "0x00800000", "IN_ONLYDIR",
        "IN_DONT_FOLLOW", "IN_EXCL_UNLINK", "0x08000000", "0x10000000",
        "IN_MASK_ADD", "IN_ISDIR", "IN_ONESHOT"
    };
    /* go through each bit, printing any that are set */
    int bit;
    printf("0x%08x", ev->mask);
    for(bit = 0; bit < 32; bit++) {
        if(ev->mask & (1 << bit)) {
            printf(",%s", bitname[bit]);
        }
    }
    printf("\t");

    /* cookie */
    printf("%d\t", ev->cookie);

    /* name */
    printf("%s\n", ev->len ? ev->name : "");
}
