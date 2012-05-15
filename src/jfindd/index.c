/* Index the filesystem for jfindd
 *
 * James Stanley 2012
 */

#include "jfindd.h"

static void _indexfs(TreeNode *root, char *path);

/* return 1 if path is a directory, 0 if it is a non-directory and -1 if an
 * error occurred
 */
int isdir(const char *path) {
	struct stat buf;

	if(lstat(path, &buf) == -1)
		return -1;

	return S_ISDIR(buf.st_mode) && !S_ISLNK(buf.st_mode);
}

/* index the filesystem starting from the given path; print something to stderr
 * and return -1 if there are
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

	/* if the path is a directory, make it so and index under it */
	int dir;
	if((dir = isdir(path)) == -1) {
		fprintf(stderr, "stat: %s: %s\n", path, strerror(errno));
		return -1;
	} else if(dir) {
		t->dir = new_dirinfo(t);
		_indexfs(t, path);
	}

	return 0;
}

/* recursively index the filesystem starting from the given node and path;
 * path is modified but is restored to its original state
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
			fprintf(stderr, "stat %s: %s\n", path, strerror(errno));
			continue;
		} else if(dir) {
			child->dir = new_dirinfo(child);
			_indexfs(child, path);
		}
	}

	/* TODO: should we handle inotify events here? this can run for a very long
	 * time indeed and it would be a shame if the inotify queue became full
	 * just because we failed to act on new events
	 */

	*endpath = '\0';
}
