LDFLAGS+=-luv
CFLAGS+=-g
all: server client
server: check.o server.o activity.o
client: client.o
clean:
	git clean -fdx
