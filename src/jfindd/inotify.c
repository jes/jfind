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

	/* add the watch, and store it in the hash table if successful */
	if((t->dir->wd = inotify_add_watch(ifd, path, WATCH_MASK)) == -1)
		fprintf(stderr, "inotify_add_watch: %s: %s\n", path, strerror(errno));
	else
		set_treenode_for_wd(t->dir->wd, t);
}

/* deal with any new inotify events */
void handle_inotify_events(void) {
	char buf[4096];

	/* TODO: poll with 0 timeout to check that there is actually data? */

	/* read from the inotify fd */
	int n;
	if((n = read(ifd, buf, 1024)) <= 0) {
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

		/* lookup the node this wd describes */
		TreeNode *t = treenode_for_wd(ev->wd);

		/* don't do anything if we are being told to ignore a node we are
		 * already ignoring
		 */
		if(!t) {
			if(ev->mask != IN_IGNORED) {
				fprintf(stderr, "warning: \n");
			}
			continue;
		}

		if(ev->mask & IN_CREATE)/* new node created */
			_inotify_create(t, ev);
		else if(ev->mask & IN_DELETE)/* node deleted */
			_inotify_delete(t, ev);
		else if(ev->mask & IN_MOVED_FROM)/* node moved away from somewhere */
			_inotify_moved_from(t, ev);
		else if(ev->mask & IN_MOVED_TO)/* node moved to somewhere */
			_inotify_moved_to(t, ev);
		else {
			fprintf(stderr, "error: received inotify event with unknown mask "
					"0x%08x!\n", ev->mask);
			exit(1);
		}
	}
}

/* handle an IN_CREATE event */
void _inotify_create(TreeNode *parent, struct inotify_event *ev) {
	assert(ev->mask & IN_CREATE);/* has to be IN_CREATE */

	TreeNode *new = new_treenode(ev->name);

	char *indexfrom = malloc(strlen(name) + strlen(ev->name) + 1);
	sprintf(indexfrom, "%s%s", name, ev->name);

	/* TODO: finish this */
}
