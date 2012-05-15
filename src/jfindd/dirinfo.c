/* DirInfo manipulation for jfind
 *
 * James Stanley 2012
 */

#include "jfind.h"

DirInfo *wd_hash;

/* allocate and return a DirInfo for the given TreeNode */
DirInfo *new_dirinfo(TreeNode *t) {
    DirInfo *d = malloc(sizeof(DirInfo));

    memset(d, 0, sizeof(DirInfo*));
    d->t = t;
    d->wd = -1;

    return d;
}

/* return the DirInfo for the given watch descriptor */
DirInfo *dirinfo_for_wd(int wd) {
    DirInfo *d;

    HASH_FIND_INT(wd_hash, &wd, d);

    return d;
}

/* free the given DirInfo (and all of the child TreeNodes) */
void free_dirinfo(DirInfo *d) {
    if(!d)
        return;

    int i;
    for(i = 0; i < d->nchilds; i++)
        free_treenode(d->child[i]);
    free(d->child);

    if(d->wd != -1)
        HASH_DEL(wd_hash, d);

    d->t->dir = NULL;
    free(d);
}
