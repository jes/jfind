# Makefile for jfind
# James Stanley 2012

CFLAGS=-g -Wall
LDFLAGS=
jfindd_OBJS=src/daemon/jfindd.o src/daemon/treenode.o src/daemon/dirinfo.o \
			src/daemon/index.o src/daemon/inotify.o src/daemon/nodemove.o \
            src/daemon/socket.o src/daemon/string.o

all: jfindd

clean:
	-rm -f jfindd $(jfindd_OBJS)

jfindd: $(jfindd_OBJS)
	$(CC) -o jfindd $(jfindd_OBJS) $(LDFLAGS)
