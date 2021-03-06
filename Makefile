# Makefile
# Copyright (C) 2008 Loren B. Davis.
# Released under the LGPL, version 3.  See COPYING.

VERSION = 1.0.1
PACKAGE = portland-doors

CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
AR = $(CROSS_COMPILE)ar
RANLIB = $(CROSS_COMPILE)ranlib

CFLAGS		+= -g -pipe -D_FILE_OFFSET_BITS=64 -std=c99 \
-combine -pthread

WARNINGS	= -Wall -Wstrict-prototypes -Wsign-compare -Wshadow \
		  -Wchar-subscripts -Wmissing-declarations -Wnested-externs \
		  -Wpointer-arith -Wcast-align -Wsign-compare -Wmissing-prototypes \
		  -pedantic
IFLAGS		= -I include

CFLAGS		+= $(WARNINGS)
CFLAGS		+= $(IFLAGS)
LDFLAGS		+= -Wl,-warn-common,--as-needed

PREFIX		= /usr/local
# PREFIX	= /usr
LIBPATH		= $(PREFIX)/lib
HEADERPATH	= $(PREFIX)/include

# On GCC, set CFLAGS to include -std=c99 -m64 -Wall -pedantic
# On Linux, set LIBS to -pthread
# On Solaris, set LIBS to -lpthread -lsocket

PROGRAMS = 	test/error1		\
		test/localserver1	\
		test/localserver2	\
		test/get_unique_id	\
		test/client-server2	\
		test/client-server3	\
		test/client-server4	\
		test/door_call1		\
		test/sun2		\
		test/unref1		\
		test/unref2

DOOR_OBJS =	door.o

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

install: include/door.h libdoor.so libdoor.a
	install -c libdoor.so $(LIBPATH)/libdoor.so.$(VERSION)
	ln -fs $(LIBPATH)/libdoor.so.$(VERSION) $(LIBPATH)/libdoor.so
	ln -fs $(LIBPATH)/libdoor.so.$(VERSION) $(LIBPATH)/libdoor.so.1
	install -c libdoor.a $(LIBPATH)/libdoor.a
	install -c include/door.h $(HEADERPATH)/door.h
	touch install

# build the objects
%.o: %.c $(HEADERS)
	$(E) "  CC	" $@
	$(Q) $(CC) -c $(CFLAGS) $< -o $@


# build stuff by hand.  This needs to be cleaned up into the core library and
# the individual test programs.

libdoor.a: $(DOOR_OBJS)
	$(Q) rm -f $@
	$(E) "  AR      " $@
	$(Q) $(AR) cq $@ $(DOOR_OBJS)
	$(E) "  RANLIB  " $@
	$(Q) $(RANLIB) $@

libdoor.so: door.lo
	libtool --mode=link $(CC) $(CFLAGS) $(DEBUGFLAGS) -o libdoor.so door.lo -shared -dynamic -rpath $(LIBPATH)

door.lo: door.c include/door.h include/standards.h include/error.h include/messages.h
	libtool --mode=compile $(CC) $(CFLAGS) $(DEBUGFLAGS) -c door.c

test/get_unique_id: test/get_unique_id.o libdoor.a
	$(E) "  CC	" $@
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/get_unique_id \
test/get_unique_id.o libdoor.a

test/sun1: test/sun1.o libdoor.a
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/sun1 \
test/sun1.o libdoor.a

test/sun2: test/sun2.o libdoor.a
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/sun2 \
test/sun2.o libdoor.a

test/error1: test/error1.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/error1 \
test/error1.o

test/localserver1: test/localserver1.o libdoor.a
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/localserver1 \
test/localserver1.o libdoor.a

test/localserver2: test/localserver2.o libdoor.a
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/localserver2 \
test/localserver2.o libdoor.a

test/socketpair1: test/socketpair1.o libdoor.a
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o test/socketpair1 \
test/socketpair1.o libdoor.a

test/client-server2: test/client-server2.o libdoor.a
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) \
-o test/client-server2 test/client-server2.o libdoor.a

test/client-server3: test/client-server3.o libdoor.a
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) \
-o test/client-server3 test/client-server3.o libdoor.a

test/client-server4: test/client-server4.o libdoor.a
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) \
-o test/client-server4 test/client-server4.o libdoor.a

test/door_call1: test/door_call1.o libdoor.a
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) \
-o test/door_call1 test/door_call1.o libdoor.a

test/unref1: test/unref1.o libdoor.a
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) \
-o test/unref1 test/unref1.o libdoor.a

test/unref2: test/unref2.o libdoor.a
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) \
-o test/unref2 test/unref2.o libdoor.a

# Obsolete:
test/client-server1: test/client-server1.o libdoor.a
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) $(LIBS) \
-o test/client-server1 test/client-server1.o libdoor.a

clean:
	$(E) "  CLEAN"
	$(Q) -rm -f *.o test/*.o $(PROGRAMS) $(DOOR_OBJS) $(OBJS) \
libdoor.la libdoor.a libdoor.so* *.lo .libs/* install
.PHONY: clean

release:
	$(Q) - rm -f $(PACKAGE).tar.gz
	# head -1 ChangeLog | grep -q "to v$(VERSION)"
	# head -1 RELEASE-NOTES | grep -q "smugbatch $(VERSION)"
	# git commit -a -m "release $(VERSION)"
	# cat .git/refs/heads/master > .git/refs/tags/$(VERSION)
	@ echo
	git-archive --format=tar --prefix=$(PACKAGE)-$(VERSION)/ HEAD | gzip -a -v --best > $(PACKAGE)-$(VERSION).tar.gz
.PHONY: release
