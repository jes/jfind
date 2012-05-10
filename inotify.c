/* Test inotify */

#include <sys/inotify.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int main(int argc, char **argv) {
    int i;
    int ifd;
    int *descrs;
    char buf[4096];
    struct inotify_event *ev[1 + 4096 / sizeof(struct inotify_event)];
    int dirmask = IN_ACCESS | IN_MODIFY | IN_ATTRIB | IN_OPEN | IN_CLOSE_WRITE
        | IN_CLOSE_NOWRITE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF;
    int filemask = dirmask & ~(IN_DELETE | IN_CREATE);

    if(argc == 1) {
        fprintf(stderr, "usage: inotify FILES...\n");
        return 1;
    }

    if((ifd = inotify_init()) == -1) {
        perror("inotify_init");
        return 1;
    }

    descrs = malloc(sizeof(int) * (argc - 1));

    for(i = 1; i < argc; i++) {
        if((descrs[i-1] = inotify_add_watch(ifd, argv[i], dirmask)) == -1) {
            fprintf(stderr, "inotify_add_watch(%s): %s\n", argv[i],
                    strerror(errno));
            if((descrs[i-1] = inotify_add_watch(ifd, argv[i], filemask))
                    == -1) {
                fprintf(stderr, "inotify_add_watch(%s): %s\n", argv[i],
                        strerror(errno));
            } else {
                printf("ws %d = %s\n", descrs[i-1], argv[i]);
            }
        } else {
            printf("wd %d = %s\n", descrs[i-1], argv[i]);
        }
    }

    ssize_t n;
    while((n = read(ifd, &buf, sizeof(buf))) > 0) {
        int nevents = 0;
        int p = 0;
        while(p < n) {
            ev[nevents] = buf + p;
            p += ev[nevents]->len + sizeof(struct inotify_event);

            nevents++;
        }

        for(i = 0; i < nevents; i++) {
            int j;
            char *name = NULL;
            for(j = 1; j < argc; j++) {
                if(descrs[j-1] == ev[i]->wd) {
                    name = argv[j];
                    break;
                }
            }
            printf("wd %d (%s):\n", ev[i]->wd, name);

            printf("  events: %d = ", ev[i]->mask);
            if(ev[i]->mask & IN_ACCESS)
                printf(" IN_ACCESS");
            if(ev[i]->mask & IN_MODIFY)
                printf(" IN_MODIFY");
            if(ev[i]->mask & IN_ATTRIB)
                printf(" IN_ATTRIB");
            if(ev[i]->mask & IN_OPEN)
                printf(" IN_OPEN");
            if(ev[i]->mask & IN_CLOSE_WRITE)
                printf(" IN_CLOSE_WRITE");
            if(ev[i]->mask & IN_CLOSE_NOWRITE)
                printf(" IN_CLOSE_NOWRITE");
            if(ev[i]->mask & IN_MOVED_FROM)
                printf(" IN_MOVED_FROM");
            if(ev[i]->mask & IN_MOVED_TO)
                printf(" IN_MOVED_TO");
            if(ev[i]->mask & IN_DELETE)
                printf(" IN_DELETE");
            if(ev[i]->mask & IN_CREATE)
                printf(" IN_CREATE");
            if(ev[i]->mask & IN_DELETE_SELF)
                printf(" IN_DELETE_SELF");
            printf("\n");

            printf("  cookie: %d\n", ev[i]->cookie);

            if(ev[i]->len > 0)
                printf("  name: %s\n", ev[i]->name);
            else
                printf("  name: (null)\n");
        }
    }

    return 0;
}
