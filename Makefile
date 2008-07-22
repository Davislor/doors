# Makefile
# Copyright (C) 2008 Loren B. Davis.
# Released under the LGPL, version 3.  See COPYING.

VERSION = 001
PACKAGE = portland_doors

CC = gcc

CFLAGS		+= -g -pipe -D_FILE_OFFSET_BITS=64 -O2 -std=c99 -combine

WARNINGS	= -Wall -Wstrict-prototypes -Wsign-compare -Wshadow \
		  -Wchar-subscripts -Wmissing-declarations -Wnested-externs \
		  -Wpointer-arith -Wcast-align -Wsign-compare -Wmissing-prototypes \
		  -pedantic
IFLAGS		= -I include

CFLAGS		+= $(WARNINGS)
CFLAGS		+= $(IFLAGS)
LDFLAGS		+= -Wl,-warn-common,--as-needed

INSTALLPATH	= /usr/local/lib

# On GCC, set CFLAGS to include -std=c99 -m64 -Wall -pedantic
# On Linux, set LIBS to -pthread
# On Solaris, set LIBS to -lpthread -lsocket
LIBS = -pthread

# what are the actual library files here?

PROGRAMS = 	test/error1		\
		test/localserver2	\
		test/get_unique_id	\
		test/socketpair1	\
		test/client-server2	\
		test/door_call1

DOOR_OBJS =	door_server.lo	\
		door_client.lo	\
		error.lo

OBJS =		test/get_unique_id.o	\
		test/error1.o		\
		test/sun1.o

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

libdoor.la: door_client.lo door_server.lo error.lo
	libtool --mode=link \
$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) -o libdoor.la \
door_client.lo door_server.lo error.lo -rpath /usr/local/lib

door_client.lo: door_client.c include/standards.h include/door.h \
include/error.h include/messages.h
	libtool --mode=compile $(CC) $(CFLAGS) $(DEBUGFLAGS) -c \
door_client.c

door_server.lo: door_server.c include/standards.h include/door.h \
include/error.h include/messages.h
	libtool --mode=compile $(CC) $(CFLAGS) $(DEBUGFLAGS) -c \
door_server.c

error.lo: include/standards.h include/error.h error.c
	libtool --mode=compile $(CC) $(CFLAGS) $(DEBUGFLAGS) -c \
error.c

test/get_unique_id: test/get_unique_id.o error.o
	$(E) "  CC	" $@
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/get_unique_id \
test/get_unique_id.o error.o

test/sun1: test/sun1.o error.o door_server.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/sun1 \
test/sun1.o error.o door_server.o

test/error1: error.o test/error1.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/error1 test/error1.o error.o

test/localserver1: test/localserver1.o door_server.o test/error.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/localserver1 \
test/localserver1.o door_server.o error.o

test/localserver2: test/localserver2.o door_server.o error.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/localserver2 \
test/localserver2.o door_server.o error.o

test/socketpair1: test/socketpair1.o door_server.o error.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/socketpair1 \
test/socketpair1.o door_server.o error.o

test/client-server1: test/client-server1.o error.o door_client.o door_server.o
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) \
-o test/client-server1 test/client-server1.o door_client.o door_server.o error.o

test/client-server2: test/client-server2.o door_client.o door_server.o error.o
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) \
-o test/client-server2 test/client-server2.o door_client.o door_server.o error.o

test/door_call1: test/door_call1.o door_client.o door_server.o error.o
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) \
-o test/door_call1 test/door_call1.o door_client.o door_server.o error.o

clean:
	$(E) "  CLEAN"
	$(Q) -rm -f *.o test/*.o $(PROGRAMS) $(DOOR_OBJS) $(OBJS) \
libdoor.la
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
