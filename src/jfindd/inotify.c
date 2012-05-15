/* Handle inotify for jfindd
 *
 * James Stanley 2012
 */

#include "jfindd.h"

#define WATCH_MASK (IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO)

static int ifd;

/* initialise inotify, printing a message and dying if there is a problem */
void init_inotify(void) {
	if((ifd = inotify_init()) == -1) {
		perror("inotify_init");
		exit(1);
	}
}

/* watch the given directory (corresponding to the given node) with inotify;
 * print an error and return as normal if watching fails
 */
void watch_directory(TreeNode *t, const char *path) {
	assert(t->dir);/* the node must be a directory */

	if((t->dir->wd = inotify_add_watch(ifd, path, WATCH_MASK)) == -1)
		fprintf(stderr, "inotify_add_watch: %s: %s\n", path, strerror(errno));
}
