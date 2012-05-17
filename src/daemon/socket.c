/* Handle clients and the main loop for jfindd
 *
 * James Stanley 2012
 */

#include "jfindd.h"

static ClientBuffer *fd_hash;

/* bind to a unix socket and run the main loop processing inotify events and
 * giving search results to clients
 * NOTE: sockpath must fit in sockaddr_un.sun_path, so must be no more than 107
 * bytes long, plus a terminating nul byte
 */
void run(TreeNode *root, const char *sockpath) {
    int sockfd;

    /* make a socket */
    if((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_un local;
    local.sun_family = AF_UNIX;

    assert(strlen(sockpath) < 108);/* there is only space for 108 bytes */
    strcpy(local.sun_path, sockpath);

    /* unlink anything that is already at that path and then bind */
    unlink(sockpath);
    if(bind(sockfd, (struct sockaddr *)&local,
            strlen(local.sun_path) + sizeof(local.sun_family)) == -1) {
        perror("bind");
        exit(1);
    }

    /* listen on this fd */
    if(listen(sockfd, 5) == -1) {
        perror("listen");
        exit(1);
    }

#define MAXPOLLFDS 256
    struct pollfd fds[MAXPOLLFDS];
    int nfds;

    fds[0].fd = inotify_fd;
    fds[0].events = POLLIN;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN;
    nfds = 2;

    while(1) {
        /* wait for input on any of the fds */
        if(poll(fds, nfds, -1) == -1) {
            perror("poll");
            break;
        }

        /* handle events */
        int i;
        int ndeleted = 0;
        for(i = 0; i < nfds; i++) {
            /* die if there is a problem */
            /* TODO: handle this; when can it happen? */
            if(fds[i].revents & POLLERR) {
                fprintf(stderr, "error: read error on pollfd %d (.fd=%d)\n",
                        i, fds[i].fd);
                exit(1);
            }

            /* stream closed */
            if(fds[i].revents & POLLHUP) {
                /* die if the inotify or listening socket fd are closed */
                if(i < 2) {
                    fprintf(stderr, "error: pollfd %d (%s) closed (.fd=%d)\n",
                            i, (i == 0 ? "inotify" : "socket"), fds[i].fd);
                    exit(1);
                }

                /* close the client fd and mark it as unused */
                close(fds[i].fd);
                clear_clientbuffer(fds[i].fd);
                fds[i].fd = -1;
            }

            /* data available */
            if(fds[i].revents & POLLIN) {
                if(i == 0) {
                    /* inotify events */
                    handle_inotify_events(root);
                } else if(i == 1) {
                    /* connection from client */
                    struct sockaddr_un remote;
                    socklen_t len = sizeof(struct sockaddr_un);

                    int fd = accept(sockfd, (struct sockaddr *)&remote, &len);

                    if(nfds == MAXPOLLFDS) {
                        fprintf(stderr, "warning: had to disconnect a client "
                                "because we already have %d open fds (try "
                                "increasing MAXPOLLFDS in " __FILE__ ")\n",
                                MAXPOLLFDS);
                        /* TODO: write some error message to the socket */
                        close(fd);
                    } else {
                        fds[nfds].fd = fd;
                        fds[nfds].events = POLLIN;
                        fds[nfds].revents = 0;
                        nfds++;
                    }
                } else {
                    /* data from client */
                    if(handle_client_data(root, fds[i].fd) == -1) {
                        close(fds[i].fd);
                        clear_clientbuffer(fds[i].fd);
                        fds[i].fd = -1;
                    }
                }
            }

            /* shuffle this pollfd along to overwrite deleted ones */
            fds[i - ndeleted] = fds[i];

            /* if this pollfd was deleted, shuffle future ones along by one
             * extra place
             */
            if(fds[i].fd == -1)
                ndeleted++;
        }

        nfds -= ndeleted;
    }
}

/* allocate and initialise a new ClientBuffer for the given fd */
ClientBuffer *new_clientbuffer(int fd) {
    ClientBuffer *c = malloc(sizeof(ClientBuffer));

    memset(c, 0, sizeof(ClientBuffer));

    c->fd = fd;
    c->buf = malloc(1024);
    c->nallocd = 1024;

    return c;
}

/* free the buffer associated with this fd */
void clear_clientbuffer(int fd) {
    ClientBuffer *c;

    HASH_FIND_INT(fd_hash, &fd, c);

    /* if there isn't a buffer for this fd, do nothing */
    if(!c)
        return;

    /* remove from the hash and free up memory */
    HASH_DEL(fd_hash, c);
    free(c->buf);
    free(c);
}

static int search_fd;
static char *search_term;

/* callback for traverse() to give search results to clients */
/* TODO: if this is slow, build it into traverse() */
/* TODO: regex search */
static int search(const char *path) {
    if(strstr(path, search_term)) {
        write(search_fd, path, strlen(path));
        char nl = '\n';
        write(search_fd, &nl, 1);
    }

    return 0;
}

/* read and buffer data from a client, and when an endline is encountered do
 * the search
 * return 0 on success and -1 if the client is disconnected
 */
int handle_client_data(TreeNode *root, int fd) {
    ClientBuffer *c;

    HASH_FIND_INT(fd_hash, &fd, c);

    /* if there isn't a buffer yet, make one */
    if(!c) {
        c = new_clientbuffer(fd);
        HASH_ADD_INT(fd_hash, fd, c);
    }

    /* grow the buffer if it is full */
    if(c->nbytes == c->nallocd) {
        c->nallocd *= 2;
        c->buf = realloc(c->buf, c->nallocd);
    }

    /* read into the buffer */
    int n;
    if((n = read(c->fd, c->buf + c->nbytes, c->nallocd - c->nbytes - 1))
            <= 0) {
        if(n < 0)
            perror("read");
        return -1;
    }

    c->nbytes += n;
    c->buf[c->nbytes] = '\0';

    /* if we get an endline, do a search */
    char *end;
    while((end = strchr(c->buf, '\n'))) {
        *end = '\0';

        /* set up information for search() callback */
        search_fd = c->fd;
        search_term = c->buf;

        /* do the search */
        /* TODO: timing */
        traverse(root, "/", search);

        /* write a final endline to the client */
        char nl = '\n';
        write(c->fd, &nl, 1);

        /* move the rest of the buffer back to the start */
        memmove(c->buf, end+1, c->nbytes + c->buf - end + 1);
        c->nbytes -= end - c->buf + 1;
    }

    return 0;
}
