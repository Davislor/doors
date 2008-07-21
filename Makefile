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

OBJS =		test/get_unique_id.o	\
		test/error1.o		\
		test/sun1.o		\
		door_server.o		\
		door_client.o		\
		error.o

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

test/get_unique_id: test/get_unique_id.o error.o
	$(E) "  CC	" $@
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/get_unique_id \
test/get_unique_id.o error.o

test/get_unique_id.o: test/get_unique_id.c include/door.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -o test/get_unique_id.o -c test/get_unique_id.c

test/sun1: test/sun1.o error.o door_server.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/sun1 \
test/sun1.o error.o door_server.o

test/error1: error.o test/error1.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o error1 test/error1.o error.o

test/error1.o: test/error1.c include/door.h include/standards.h include/error.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -o test/error1.o -c test/error1.c

test/localserver1: test/localserver1.o door_server.o test/error.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/localserver1 \
test/localserver1.o door_server.o error.o

test/localserver1.o: test/localserver1.c include/door.h include/error.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -o test/localserver1.o -c test/localserver1.c

test/localserver2: test/localserver2.o door_server.o error.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/localserver2 \
test/localserver2.o door_server.o error.o

test/localserver2.o: test/localserver1.c include/door.h include/error.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -o test/localserver2.o -c test/localserver2.c

test/socketpair1: test/socketpair1.o door_server.o error.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/socketpair1 \
test/socketpair1.o door_server.o error.o

test/socketpair1.o: test/socketpair1.c include/door.h include/messages.h \
include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -o test/socketpair1.o -c test/socketpair1.c

test/client-server1.o: test/client-server1.c include/door.h include/messages.h \
include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -o test/client-server1.o -c test/client-server1.c

test/client-server1: test/client-server1.o error.o door_client.o door_server.o
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) \
-o test/client-server1 test/client-server1.o door_client.o door_server.o error.o

test/client-server2.o: test/client-server2.c include/door.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -o test/client-server2.o -c test/client-server2.c

test/client-server2: test/client-server2.o door_client.o door_server.o error.o
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) \
-o test/client-server2 test/client-server2.o door_client.o door_server.o error.o

test/door_call1: test/door_call1.o door_client.o door_server.o error.o
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) \
-o test/door_call1 test/door_call1.o door_client.o door_server.o error.o

test/door_call1.o: test/door_call1.c include/door.h include/error.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -o test/door_call1.o -c test/door_call1.c

clean:
	$(E) "  CLEAN"
	$(Q) -rm -f *.o $(PROGRAMS) $(OBJS)
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

