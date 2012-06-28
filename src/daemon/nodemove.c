/* Handle node moves (renames) for jfindd
 *
 * James Stanley 2012
 */

#include "jfindd.h"

static NodeMove *move_hash;

/* allocate and return a new NodeMove */
NodeMove *new_nodemove(void) {
    NodeMove *m = malloc(sizeof(NodeMove));

    memset(m, 0, sizeof(NodeMove));

    return m;
}

/* remember the node corresponding to the given cookie so that it can be
 * renamed later
 */
void set_node_moved_from(int cookie, TreeNode *t) {
    NodeMove *m = new_nodemove();

    m->cookie = cookie;
    m->node = t;

    HASH_ADD_INT(move_hash, cookie, m);
}

/* return the TreeNode associated with the given cookie, and then forget the
 * association and free the NodeMove (!)
 */
TreeNode *node_for_cookie(int cookie) {
    NodeMove *m;
    TreeNode *t;

    HASH_FIND_INT(move_hash, &cookie, m);

    assert(m);/* the NodeMove must be in the hash */

    t = m->node;

    HASH_DEL(move_hash, m);
    free(m);

    return t;
}
