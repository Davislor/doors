# Makefile
# Copyright (C) 2008 Loren B. Davis.
# Released under the LGPL, version 3.  See COPYING.

CC = c99 -I include
DEBUGFLAGS = -g
PRODUCTIONFLAGS = -O3

# On Sun C, set CFLAGS to include -xtarget=native64 -mt
# On GCC, set CFLAGS to include -std=c99 -m64 -Wall -pedantic
# On Linux, set LDFLAGS to -pthread
# On Solaris, set LDFLAGS to -lpthread -lsocket

clean:
	rm localserver1.o localserver1 localserver2.o localserver2 \
sun1.o sun1 sun2.o sun2 error1.o error1 door_server.o error.o \
door_client.o get_unique_id.o get_unique_id

tests: sun1 error1 localserver1 localserver2 sun2 get_unique_id

get_unique_id: get_unique_id.o error.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(DEBUGFLAGS) -o get_unique_id \
get_unique_id.o error.o

get_unique_id.o: test/get_unique_id.c include/door.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c test/get_unique_id.c

sun1: sun1.o error.o door_server.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(DEBUGFLAGS) -o sun1 sun1.o \
error.o door_server.o

sun1.o: test/sun1.c include/door.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c test/sun1.c

error1: error.o error1.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(DEBUGFLAGS) -o error1 error.o \
error1.o

error1.o: test/error1.c include/door.h include/standards.h include/error.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c test/error1.o

error.o: error.c include/error.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c error.c

door_server.o: door_server.c include/door.h include/error.h \
include/standards.h include/messages.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c door_server.c

localserver1: localserver1.o door_server.o error.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(DEBUGFLAGS) -o localserver1 \
localserver1.o door_server.o error.o

localserver1.o: test/localserver1.c include/door.h include/error.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c test/localserver1.c

localserver2: localserver2.o door_server.o error.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(DEBUGFLAGS) -o localserver2 \
localserver2.o door_server.o error.o

localserver2.o: test/localserver1.c include/door.h include/error.h include/standards.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -c test/localserver2.c

