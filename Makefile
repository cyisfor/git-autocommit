LDFLAGS+=-O2 -luv
CFLAGS+=-O2
all: server client
server: check.o server.o activity.o
client: client.o
clean:
	git clean -fdx
