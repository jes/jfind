/* Index the filesystem for jfindd
 *
 * James Stanley 2012
 */

#include "jfindd.h"

static void _indexfs(TreeNode *root, char *path);
static int _traverse(TreeNode *t, char *path, TraversalFunc callback);

/* return 1 if path is a directory, 0 if it is a non-directory and -1 if an
 * error occurred
 */
int isdir(const char *path) {
    struct stat buf;

    if(lstat(path, &buf) == -1) {
        fprintf(stderr, "stat: %s: %s\n", path, strerror(errno));
        return -1;
    }

    return S_ISDIR(buf.st_mode) && !S_ISLNK(buf.st_mode);
}

/* reindex anything in the tree that has indexed=0 */
void reindex(TreeNode *node, TreeNode *root) {
    /* if this node isn't indexed, find out its name and index it, otherwise
     * recurse on its children
     */
    if(!node->indexed) {
        char *name = treenode_name(node);
        printf("%s is not indexed!\n", name);
        indexfrom(root, name);
        free(name);
    } else if(node->dir) {
        int i;
        for(i = 0; i < node->dir->nchilds; i++)
            reindex(node->dir->child[i], root);
    }
}

/* index the filesystem starting from the given path; print something to stderr
 * and return -1 if there are errors
 */
int indexfrom(TreeNode *root, const char *relpath) {
    char path[PATH_MAX];

    assert(!root->parent);/* root should be actual root */

    /* get an absolute path */
    if(!realpath(relpath, path)) {
        fprintf(stderr, "error: realpath: %s\n", strerror(errno));
        return -1;
    }

    /* get a node describing this path */
    TreeNode *t;
    if(!(t = create_path(root, path))) {
        fprintf(stderr, "error: adding %s to tree: Not a directory\n",
                path);
        return -1;
    }

    /* mark everything above the node as indexed so that it doesn't get
     * indexed later
     */
    TreeNode *node;
    for(node = t->parent; node; node = node->parent)
        node->indexed = 1;

    /* if the path is a directory, make it so and index under it */
    int dir;
    if((dir = isdir(path)) == -1) {
        return -1;
    } else if(dir) {
        t->dir = new_dirinfo(t);

        /* remove a trailing slash if there is one (note: "/" -> "" but that's
         * OK)
         */
        if(*path && path[strlen(path)-1] == '/')
            path[strlen(path)-1] = '\0';
        _indexfs(t, path);
    }

    return 0;
}

/* recursively index the filesystem starting from the given node and path;
 * path is modified but is restored to its original state, and should have at
 * least PATH_MAX bytes of storage
 */
static void _indexfs(TreeNode *root, char *path) {
    char *endpath = path + strlen(path);

    assert(root->dir);/* can't index under a non-directory */

    /* ensure the path is not too long */
    if(strlen(path) >= PATH_MAX-1) {
        fprintf(stderr, "error: %s: strlen(path) too long!\n", path);
        exit(1);
    }
    strcat(path, "/");

    DIR *dp;
    if(!(dp = opendir(path))) {
        fprintf(stderr, "opendir: %s: %s\n", path, strerror(errno));
        return;
    }

    /* watch this path with inotify */
    watch_directory(root, path);

    /* loop over all of the entries in the directory */
    struct dirent *de;
    while((de = readdir(dp))) {
        if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        /* end the path at the right place */
        *(endpath + 1) = '\0';

        /* check that de->d_name is short enough to append to path */
        if(strlen(de->d_name) > PATH_MAX - 1 - strlen(path)) {
            fprintf(stderr, "error: %s: %s: strlen(de->d_name) too long!\n",
                    path, de->d_name);
            exit(1);
        }
        strcat(path, de->d_name);

        /* add a new node to the tree */
        TreeNode *child = new_treenode(de->d_name);
        add_child(root, child);

        /* if this node is a directory, recurse */
        int dir;
        if((dir = isdir(path)) == -1) {
            continue;
        } else if(dir) {
            child->dir = new_dirinfo(child);
            _indexfs(child, path);
        } else {
            /* non-directories need no further work */
            child->indexed = 1;
        }
    }

    root->indexed = 1;

    closedir(dp);

    /* TODO: should we handle inotify events here? this can run for a very long
     * time indeed and it would be a shame if the inotify queue became full
     * just because we failed to act on new events
     */

    *endpath = '\0';
}

/* traverse the tree depth-first, starting at path, and call the callback for
 * every node;
 * if callback returns non-zero, the traversal will be halted;
 * returns 0 on a full tree traversal, and returns the value returned by the
 * callback in the case that the traversal is halted prematurely;
 * returns -1 on error (i.e. "path" is not in the tree or is too long)
 */
int traverse(TreeNode *root, const char *path, TraversalFunc callback) {
    assert(!root->parent);/* this should be actual root */

    /* fail if the path is too long, and copy it if it is ok */
    if(strlen(path) >= PATH_MAX)
        return -1;
    char newpath[PATH_MAX];
    strcpy(newpath, path);

    /* remove a trailing slash if appropriate */
    if(*newpath && newpath[strlen(newpath)-1] == '/')
        newpath[strlen(newpath)-1] = '\0';

    /* lookup the node and fail if there is no such node */
    TreeNode *t = lookup_treenode(root, newpath);
    if(!t)
        return -1;

    /* do the actual traversal */
    return _traverse(t, newpath, callback);
}

/* traverse the entire tree depth-first, calling the callback for every path;
 * path will be modified but will be restored to its original state, and
 * should have at least PATH_MAX bytes of storage;
 * if callback returns non-zero, the traversal will be halted and the returned
 * value will be teturned;
 * returns 0 on a full tree traversal, and returns the value returned by the
 * callback in the case that the traversal is halted prematurely
 */
static int _traverse(TreeNode *t, char *path, TraversalFunc callback) {
    char *endpath = path + strlen(path);

    /* check that the name is small enough */
    if(strlen(t->name) > PATH_MAX - 2 - strlen(path)) {
        fprintf(stderr, "error: %s: %s: strlen(t->name) too long!\n",
                path, t->name);
        exit(1);
    }
    strcat(path, t->name);
    if(t->dir)
        strcat(path, "/");

    /* call the user-supplied callback */
    int n;
    if((n = callback(path)))
        return n;

    /* recurse if this is a directory */
    if(t->dir) {
        int i;
        for(i = 0; i < t->dir->nchilds; i++)
            if((n = _traverse(t->dir->child[i], path, callback)))
                return n;
    }

    *endpath = '\0';

    return 0;
}
