/* jfind client
 *
 * James Stanley 2012
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "usage: jfind search-term\n");
        return 1;
    }

    int fd;
    if((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un remote;
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, "./socket");
    size_t len = strlen(remote.sun_path) + sizeof(remote.sun_family);

    if(connect(fd, (struct sockaddr *)&remote, len) == -1) {
        perror("connect");
        return 1;
    }

    FILE *fp;
    if(!(fp = fdopen(fd, "r+"))) {
        perror("fdopen");
        close(fd);
        return 1;
    }

    fprintf(fp, "%s\n", argv[1]);

    char buf[4096];
    while(fgets(buf, 4096, fp)) {
        if(*buf == '\n')
            break;

        fputs(buf, stdout);
    }

    fclose(fp);

    return 0;
}
