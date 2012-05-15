/* Header file for jfind
 *
 * James Stanley 2012
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

/* store information for a directory
 * this is a separate structure to TreeNode in the interest of saving memory
 */
typedef struct DirInfo {
    struct TreeNode *t;/* the TreeNode this DirInfo describes */
    int wd;/* watch descriptor */
    int nchilds;
    struct TreeNode **child;
    Ut_hash_handle hh;/* for the hash table mapping wd to DirInfo */
} DirInfo;

/* store information for an arbitrary node in the fs tree */
typedef struct TreeNode {
    char indexed;/* 0 if this node needs indexing and 1 otherwise */
    struct TreeNode **parent;/* the parent node (should be a directory) */
    char *name;
    DirInfo *dir;/* directory information for non-file nodes */
} TreeNode;

/* store information for an IN_MOVED_FROM event (while IN_MOVED_FROM is
 * usually followed immediately by the corresponding IN_MOVED_TO, this is not
 * always the case)
 */
typedef struct NodeMove {
    int cookie;/* the "cookie" field from the inotify event */
    TreeNode *node;/* the node that is being moved */
    UT_hash_handle_hh;/* for the hash table mapping cookie to NodeMove */
} NodeMove;
