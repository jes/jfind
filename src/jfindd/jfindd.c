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
void help(void) {
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
int search(const char *s) {
    if(strstr(s, search_term))
        printf("%s\n", s);

    return 0;
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

    /* index all of the directories requested */
    while(optind < argc)
        indexfrom(root, argv[optind++]);

    /* set up a search interface */
    /* TODO: do this over unix domain sockets */
    char buf[1024];
    while(fgets(buf, 1024, stdin)) {
        search_buf = buf;
        if(buf[strlen(buf)-1] == '\n')
            buf[strlen(buf)-1] = '\0';
        traverse(root, "/", search);
    }

    return 0;
}
