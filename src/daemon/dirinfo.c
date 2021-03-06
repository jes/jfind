/* DirInfo manipulation for jfind
 *
 * James Stanley 2012
 */

#include "jfindd.h"

static DirInfo *wd_hash;

/* allocate and return a DirInfo for the given TreeNode */
DirInfo *new_dirinfo(TreeNode *t) {
    DirInfo *d = malloc(sizeof(DirInfo));

    memset(d, 0, sizeof(DirInfo));
    d->t = t;
    d->wd = -1;

    return d;
}

/* set the DirInfo associated with the given wd */
void set_dirinfo_for_wd(int wd, DirInfo *d) {
    assert(wd != -1);/* -1 is not a valid watch descriptor, so it is a bug */

    HASH_ADD_INT(wd_hash, wd, d);
}

/* return the DirInfo for the given watch descriptor */
DirInfo *dirinfo_for_wd(int wd) {
    DirInfo *d;

    HASH_FIND_INT(wd_hash, &wd, d);

    return d;
}

/* remove the given wd from the hash */
void remove_wd(int wd) {
    DirInfo *d = dirinfo_for_wd(wd);

    if(d)
        HASH_DEL(wd_hash, d);
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
