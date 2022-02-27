CC=gcc
INCLUDES=
LDFLAGS=-libverbs
LIBS=-pthread -lrdmacm

SRCS=teatable.c
CLIENT=client.c
SERVER=server.c
OBJS=$(SRCS:.c=.o)

all:client server
client: $(CLIENT) $(SRCS)
	$(CC) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LIBS)
server: $(SERVER) $(SRCS)
	$(CC) $(INCLUDES) $^ -o $@ $(LDFLAGS) $(LIBS)