# Makefile
# Copyright (C) 2008 Loren B. Davis.
# Released under the LGPL, version 3.  See COPYING.

VERSION = 001
PACKAGE = portland_doors

CC = gcc

CFLAGS		+= -g -pipe -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -O2 -std=c99

WARNINGS	= -Wall -Wstrict-prototypes -Wsign-compare -Wshadow \
		  -Wchar-subscripts -Wmissing-declarations -Wnested-externs \
		  -Wpointer-arith -Wcast-align -Wsign-compare -Wmissing-prototypes \
		  -pedantic
IFLAGS		= -I include

CFLAGS		+= $(WARNINGS)
CFLAGS		+= $(IFLAGS)
LDFLAGS		+= -Wl,-warn-common,--as-needed


# On GCC, set CFLAGS to include -std=c99 -m64 -Wall -pedantic
# On Linux, set LIBS to -pthread
# On Solaris, set LIBS to -lpthread -lsocket
LIBS = -pthread

# what are the actual library files here?

PROGRAMS = 	error1		\
		localserver1	\
		localserver2	\
		get_unique_id	\
		socketpair1	\
		client-server2	\
		door_call1

OBJS =		get_unique_id.o	\
		error.o		\
		error1.o	\
		sun1.o		\
		door_server.o	\

HEADERS = 	include/error.h		\
		include/door.h		\
		include/standards.h	\
		include/messages.h

ifeq ($(strip $(V)),)
	E = @echo
	Q = @
else
	E = @\#
	Q =
endif
export E Q


all: $(PROGRAMS)

# build the objects
%.o: %.c $(HEADERS)
	$(E) "  CC	" $@
	$(Q) $(CC) -c $(CFLAGS) $< -o $@


# build stuff by hand.  This needs to be cleaned up into the core library and
# the individual test programs.

get_unique_id: get_unique_id.o error.o
	$(E) "  CC	" $@
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o get_unique_id \
get_unique_id.o error.o

get_unique_id.o: test/get_unique_id.c include/door.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c test/get_unique_id.c

sun1: sun1.o error.o door_server.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o sun1 \
sun1.o error.o door_server.o

error1: error.o error1.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o error1 \
error.o error1.o

error1.o: test/error1.c include/door.h include/standards.h include/error.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c test/error1.c

door_client.o: door_client.c include/door.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c door_client.c

localserver1: localserver1.o door_server.o error.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o localserver1 \
localserver1.o door_server.o error.o

localserver1.o: test/localserver1.c include/door.h include/error.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c test/localserver1.c

localserver2: localserver2.o door_server.o error.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o localserver2 \
localserver2.o door_server.o error.o

localserver2.o: test/localserver1.c include/door.h include/error.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c test/localserver2.c

socketpair1: socketpair1.o door_server.o error.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o socketpair1 \
socketpair1.o door_server.o error.o

socketpair1.o: test/socketpair1.c include/door.h include/messages.h \
include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c test/socketpair1.c

client-server1.o: test/client-server1.c include/door.h include/messages.h \
include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c test/client-server1.c

client-server1: client-server1.o error.o door_client.o door_server.o
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) \
-o client-server1 client-server1.o door_client.o door_server.o error.o

client-server2.o: test/client-server2.c include/door.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c test/client-server2.c

client-server2: client-server2.o door_client.o door_server.o error.o
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) \
-o client-server2 client-server2.o door_client.o door_server.o error.o

door_call1: door_call1.o door_client.o door_server.o error.o
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) \
-o door_call1 door_call1.o door_client.o door_server.o error.o

door_call1.o: test/door_call1.c include/door.h include/error.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c test/door_call1.c

clean:
	$(E) "  CLEAN"
	$(Q) -rm -f *.o $(PROGRAMS)
.PHONY: clean

release:
	$(Q) - rm -f $(PACKAGE).tar.gz
	# head -1 ChangeLog | grep -q "to v$(VERSION)"
	# head -1 RELEASE-NOTES | grep -q "smugbatch $(VERSION)"
	# git commit -a -m "release $(VERSION)"
	# cat .git/refs/heads/master > .git/refs/tags/$(VERSION)
	@ echo
	git-archive --format=tar --prefix=$(PACKAGE)-$(VERSION)/ HEAD | gzip -9v > $(PACKAGE)-$(VERSION).tar.gz
.PHONY: release

