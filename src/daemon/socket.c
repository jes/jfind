/* Handle clients and the main loop for jfindd
 *
 * James Stanley 2012
 */

#include "jfindd.h"

/* bind to a unix socket and run the main loop processing inotify events and
 * giving search results to clients
 * NOTE: sockpath must fit in sockaddr_un.sun_path, so must be no more than 107
 * bytes long, plus a terminating nul byte
 */
void run(TreeNode *root, const char *sockpath) {
    int fd;

    /* make a socket */
    if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_un local;
    local.sun_family = AF_UNIX;

    assert(strlen(sockpath) < 108);/* there is only space for 108 bytes */
    strcpy(local.sun_path, sockpath);

    /* unlink anything that is already at that path and then bind */
    unlink(sockpath);
    if(bind(fd, (struct sockaddr *)&local,
            strlen(local.sun_path) + sizeof(local.sun_family)) == -1) {
        perror("bind");
        exit(1);
    }

    /* listen on this fd */
    if(listen(fd, 5) == -1) {
        perror("listen");
        exit(1);
    }

    size_t len = sizeof(struct sockaddr_un);

    /* TODO: poll */
}
