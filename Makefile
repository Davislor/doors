# Makefile
# Copyright (C) 2008 Loren B. Davis.
# Released under the LGPL, version 3.  See COPYING.

CC = gcc
CFLAGS = -std=c99
DEBUGFLAGS = -g -Wall -pedantic
LIBS = -pthread

# On GCC, set CFLAGS to include -std=c99 -m64 -Wall -pedantic
# On Linux, set LIBS to -pthread
# On Solaris, set LIBS to -lpthread -lsocket

all: error1 localserver1 localserver2 get_unique_id socketpair1 \
client-server2

get_unique_id: get_unique_id.o error.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o get_unique_id \
get_unique_id.o error.o

get_unique_id.o: test/get_unique_id.c include/door.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c test/get_unique_id.c

sun1: sun1.o error.o door_server.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o sun1 \
sun1.o error.o door_server.o

sun1.o: test/sun1.c include/door.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c test/sun1.c

error1: error.o error1.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(DEBUGFLAGS) -o error1 \
error.o error1.o

error1.o: test/error1.c include/door.h include/standards.h include/error.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c test/error1.c

error.o: error.c include/error.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c error.c

door_client.o: door_client.c include/door.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c door_client.c

door_server.o: door_server.c include/door.h include/error.h \
include/standards.h include/messages.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c door_server.c

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

clean:
	-rm -f localserver1.o localserver1 localserver2.o localserver2 \
sun1.o sun1 sun2.o sun2 error1.o error1 door_server.o error.o \
door_client.o get_unique_id.o get_unique_id socketpair1.o socketpair1 \
client-server1.o client-server1 client-server2.o client-server2
