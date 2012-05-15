# Makefile for jfind
# James Stanley 2012

CFLAGS=-g -Wall
LDFLAGS=
jfindd_OBJS=src/jfindd/jfindd.o src/jfindd/treenode.o src/jfindd/dirinfo.o \
			src/jfindd/index.o src/jfindd/inotify.o

all: jfindd

clean:
	-rm -f jfindd $(jfindd_OBJS)

jfindd: $(jfindd_OBJS)
	$(CC) -o jfindd $(jfindd_OBJS) $(LDFLAGS)
