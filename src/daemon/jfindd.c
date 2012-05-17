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

    /* handle inotify events and client requests */
    run(root, "./socket");

    return 0;
}
