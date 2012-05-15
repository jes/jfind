/* Header file for jfindd
 *
 * James Stanley 2012
 */

#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>

#include "uthash.h"

/* store information for a directory
 * this is a separate structure to TreeNode in the interest of saving memory
 */
typedef struct DirInfo {
    struct TreeNode *t;/* the TreeNode this DirInfo describes */
    int wd;/* watch descriptor */
    int nchilds;
    struct TreeNode **child;
    UT_hash_handle hh;/* for the hash table mapping wd to DirInfo */
} DirInfo;

/* store information for an arbitrary node in the fs tree */
typedef struct TreeNode {
    char indexed;/* 0 if this node needs indexing and 1 otherwise */
    struct TreeNode *parent;/* the parent node (should be a directory) */
    char *name;
    DirInfo *dir;/* directory information for non-file nodes */
} TreeNode;

/* store information for an IN_MOVED_FROM event (while IN_MOVED_FROM is
 * usually followed immediately by the corresponding IN_MOVED_TO, this is not
 * always the case)
 */
typedef struct NodeMove  {
    int cookie;/* the "cookie" field from the inotify event */
    TreeNode *node;/* the node that is being moved */
    UT_hash_handle hh;/* for the hash table mapping cookie to NodeMove */
} NodeMove;

/* treenode.c */
TreeNode *new_treenode(const char *name);
void add_child(TreeNode *t, TreeNode *child);
TreeNode *create_path(TreeNode *t, char *path);
TreeNode *lookup_treenode(TreeNode *t, char *path);
void remove_treenode(TreeNode *t);
TreeNode *remove_path(TreeNode *t, char *path);
char *treenode_name(TreeNode *t);
TreeNode *treenode_for_wd(int wd);
void free_treenode(TreeNode *t);

/* dirnode.c */
DirInfo *new_dirinfo(TreeNode *t);
void set_dirinfo_for_wd(int wd, DirInfo *d);
DirInfo *dirinfo_for_wd(int wd);
void free_dirinfo(DirInfo *d);

/* index.c */
typedef int (*TraversalFunc)(const char *);

int isdir(const char *path);
int indexfrom(TreeNode *root, const char *relpath);
int traverse(TreeNode *root, const char *path, TraversalFunc callback);

/* inotify.c */
void init_inotify(void);
void watch_directory(TreeNode *t, const char *path);
