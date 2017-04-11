LDFLAGS+=-O2 -luv -lgit2
CFLAGS+=-O2
all: testgit server client
server: check.o server.o activity.o
client: client.o
clean:
	git clean -fdx

testgit: testgit.o
