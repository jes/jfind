/* Daemon for jfind
 *
 * James Stanley 2012
 */

#include "jfindd.h"

int debug_mode = 0;
int quiet_mode = 0;
const char *socket_path = SOCKET_PATH;

static TreeNode *root;

static struct option opts[] = {
    { "debug",  no_argument,       0, 'd' },
    { "help",   no_argument,       0, 'h' },
    { "quiet",  no_argument,       0, 'q' },
    { "socket", required_argument, 0, 's' },
    { 0,        0,                 0,  0  }
};

/* --help output */
static void help(void) {
    fprintf(stderr,
    "usage: jfindd [options] paths...\n"
    "\n"
    "'Paths...' is a list of paths to index.\n"
    "\n"
    "Options:\n"
    "  -d, --debug        Output debugging information\n"
    "  -h, --help         Display this help\n"
    "  -q, --quiet        Suppress a lot of error messages\n"
    "  -s, --socket FILE  Set the path to the communication socket\n"
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
    /* parse options */
    opterr = 0;
    int c;
    while((c = getopt_long(argc, argv, "dhqs:", opts, NULL)) != -1) {
        switch(c) {
            case 'd':
                debug_mode = 1;
                break;

            case 'h':
                help();
                return 0;

            case 'q':
                quiet_mode = 1;
                break;

            case 's':
                socket_path = optarg;
                break;

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

    /* seconds to wait when something terrible happens and the filesystem needs
     * to be reindexed; this is doubled every time it happens up until
     * max_reindex_secs; the purpose of this is to prevent repeated reindexing
     * followed by immediate failure and reindexing
     */
    int reindex_secs = 5;
    int max_reindex_secs = 300;

    int init_optind = optind;

    while(1) {
        init_inotify();

        /* make the root node */
        root = new_treenode("");
        root->dir = new_dirinfo(root);

        struct timeval start, stop;

        fprintf(stderr, "Indexing...\n");

        /* index all of the directories requested */
        gettimeofday(&start, NULL);
        while(optind < argc)
            indexfrom(root, argv[optind++]);
        optind = init_optind;
        gettimeofday(&stop, NULL);

        fprintf(stderr, "Indexing took %.3fs.\n",
                difftimeofday(&start, &stop));

        /* handle inotify events and client requests */
        run(root, socket_path);
        /* if run() returns, something terrible has happened */

        /* sleep a while and then double the sleep period */
        fprintf(stderr, "warning: sleeping %d secs\n", reindex_secs);
        sleep(reindex_secs);
        reindex_secs *= 2;
        if(reindex_secs > max_reindex_secs)
            reindex_secs = max_reindex_secs;

        /* now clear all state */
        free_treenode(root);
    }

    return 0;
}
