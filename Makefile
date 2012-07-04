# Makefile for jfind
# James Stanley 2012

CFLAGS=-g -Wall
LDFLAGS=
jfindd_OBJS=src/daemon/jfindd.o src/daemon/treenode.o src/daemon/dirinfo.o \
			src/daemon/index.o src/daemon/inotify.o src/daemon/nodemove.o \
			src/daemon/socket.o src/daemon/string.o
jfind_OBJS=src/client/jfind.o

all: jfind jfindd

clean:
	-rm -f jfindd $(jfindd_OBJS)

jfindd: $(jfindd_OBJS)
	$(CC) -o jfindd $(jfindd_OBJS) $(LDFLAGS)

jfind: $(jfind_OBJS)
	$(CC) -o jfind $(jfind_OBJS) $(LDFLAGS)
