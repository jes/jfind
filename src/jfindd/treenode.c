/* TreeNode manipulation for jfind
 *
 * James Stanley 2012
 */

#include "jfind.h"

/* allocate a new treenode with the given name */
TreeNode *new_treenode(const char *name) {
    TreeNode *t = malloc(sizeof(TreeNode));

    memset(t, 0, sizeof(t));
    t->name = strdup(name);

    return t;
}

/* add the given child to the given node (which must be a directory) */
void add_child(TreeNode *t, TreeNode *child) {
    assert(t->dir);/* the parent node must be a directory */
    assert(!child->parent);/* the child node must not already have a parent */

    /* we could double the size of t->dir->child every time, but since
     * TreeNodes are only actually manipulated in I/O-bound situations, that
     * gains little and wastes quite a bit of memory
     */

    t->dir->child = realloc(t->dir->child,
            (t->dir->nchilds + 1) * sizeof(TreeNode*));
    t->dir->child[t->dir->nchilds++] = child;
    child->parent = t;
}

/* lookup the given path, starting at the given node, and return the node
 * described by the path or NULL if there is none
 * path is modified but will be restored to its original state
 */
TreeNode *lookup_treenode(TreeNode *t, char *path) {
    if(*path == '/')
        path++;

    while(*path && strcmp(path, "/") != 0) {
        if(!t->dir)/* the child node can't be found if not a directory */
            return NULL;

        /* mark the end of the next path component for strcmp's sake */
        char *endpath = NULL;
        if((endpath = strchr(path, '/')))
            *endpath = '\0';

        /* store nchilds because t gets updated to point at a different node */
        int nchilds = t->dir->nchilds;

        int i;
        for(i = 0; i < nchilds; i++) {
            if(strcmp(t->dir->child[i]->name, path) == 0) {
                /* if there are more path components, restore *endpath and
                 * move on, otherwise jump to the end of the path
                 */
                if(endpath) {
                    *endpath = '/';
                    path = endpath + 1;
                } else {
                    path += strlen(path);
                }

                /* move on to the child */
                t = t->dir->child[i];
                break;
            }
        }

        if(i == nchilds)/* no such child was found */
            return NULL;
    }

    return t;
}

/* remove the given node from the tree (remove it from its parent) but do not
 * free it
 */
void remove_treenode(TreeNode *t) {
    assert(t->parent);/* if t doesn't have a parent we can't remove it */

    /* locate this child */
    int i;
    for(i = 0; i < t->parent->dir->nchilds; i++)
        if(t->parent->dir->child[i] == t)
            break;

    assert(i != t->parent->dir->nchilds);/* the child must be found */

    /* move the remaining children towards the start of the child array by
     * one element
     */
    memmove(t->parent->dir->child + i, t->parent->dir->child + i + 1,
            sizeof(TreeNode*) * (t->parent->dir->nchilds - i - 1));

    /* reallocate the array */
    t->parent->dir->nchilds--;
    t->parent->dir->child = realloc(t->parent->dir->child,
            t->parent->dir->nchilds * sizeof(TreeNode*));
}

/* remove the node described by the given path from the tree described by t,
 * and return it
 * path is modified (by lookup_node) but is restored to its original state
 */
TreeNode *remove_path(TreeNode *t, char *path) {
    TreeNode *node = lookup_treenode(t, path);

    if(node)
        remove_treenode(node);

    return node;
}

/* allocate a name for the given node (by traversing up to the root and
 * prepending the name for the parent node) and return it
 * you must free the returned pointer
 */
char *treenode_name(TreeNode *t) {
    if(!t->parent)
        return strdup("/");

    char *path = malloc(strlen(t->name) + 2);
    sprintf(path, "/%s%s", t->name, t->dir ? "/" : "");

    t = t->parent;

    /* repeatedly prepend the parent's name */
    while(t->parent) {
        char *newpath = malloc(strlen(t->name) + strlen(path) + 2);
        sprintf(newpath, "/%s%s", t->name, path);
        free(path);
        path = newpath;
    }

    return path;
}

/* return the TreeNode for the given wd */
TreeNode *treenode_for_wd(int wd) {
    DirInfo *d = dirinfo_for_wd(wd);

    if(d)
        return d->t;
    else
        return NULL;
}

/* free the given node (and all of its children, via free_dirinfo) */
void free_treenode(TreeNode *t) {
    if(!t)
        return;

    free_dirinfo(t->dir);
    free(t->name);
    free(t);
}
