/* Daemon for jfind
 *
 * James Stanley 2012
 */

#include "jfindd.h"

static TreeNode *root;

static struct option opts[] = {
    { "help", no_argument, 0, 'h' },
    { 0,      0,           0,  0  }
};

/* --help output */
static void help(void) {
    fprintf(stderr,
    "usage: jfindd [options] paths...\n"
    "\n"
    "'Paths...' is a list of paths to index.\n"
    "\n"
    "Options:\n"
    "  -h, --help  Display this help\n"
    "\n"
    "Report bugs to James Stanley <james@incoherency.co.uk>\n"
    );
}

static char *search_term;

/* a callback for traverse() */
/* TODO: if this is slow, build it into traverse() */
static int search(const char *s) {
    if(strstr(s, search_term))
        printf("%s\n", s);

    return 0;
}

/* return the difference in seconds between *start and *stop */
static double difftimeofday(struct timeval *start, struct timeval *stop) {
    return (stop->tv_sec - start->tv_sec)
        + (stop->tv_usec - start->tv_usec) / 1000000.0;
}

int main(int argc, char **argv) {
    init_inotify();

    /* parse options */
    opterr = 0;
    int c;
    while((c = getopt_long(argc, argv, "h", opts, NULL)) != -1) {
        switch(c) {
            case 0:
                fprintf(stderr, "error: getopt_long() returned 0\n");
                return 1;

            case 'h':
                help();
                return 0;

            case '?':
                fprintf(stderr, "error: unknown option '%c'\n", optopt);
                return 1;

            default:
                fprintf(stderr, "error: getopt_long() returned 0x%02x = %c\n",
                        c, c);
                return 1;
        }
    }

    /* we need at least one path to index */
    if(optind >= argc) {
        fprintf(stderr, "usage: jfindd [options] paths...\n"
                        "See --help for more details\n");
        return 1;
    }

    /* make the root node */
    root = new_treenode("");
    root->dir = new_dirinfo(root);

    struct timeval start, stop;

    /* index all of the directories requested */
    gettimeofday(&start, NULL);
    while(optind < argc)
        indexfrom(root, argv[optind++]);
    gettimeofday(&stop, NULL);

    printf("Indexing took %.3fs.\n", difftimeofday(&start, &stop));

    /* set up a search interface */
    printf("? "); fflush(stdout);
    /* TODO: do this over unix domain sockets */
    /* TODO: poll() instead of interleaving searching and indexing */
    char buf[1024];
    while(fgets(buf, 1024, stdin)) {
        search_term = buf;
        if(buf[strlen(buf)-1] == '\n')
            buf[strlen(buf)-1] = '\0';

        gettimeofday(&start, NULL);
        traverse(root, "/", search);
        gettimeofday(&stop, NULL);
        printf("Search took %.3fms.\n\n",
                difftimeofday(&start, &stop) * 1000.0);

        printf("Doing inotify events...\n");
        handle_inotify_events(root);
        printf("\n");

        printf("? "); fflush(stdout);
    }

    free_treenode(root);

    return 0;
}
