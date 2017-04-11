OPT=-g
LDFLAGS+=$(OPT) -luv -lgit2
CFLAGS+=$(OPT)
all: testgit server client
server: check.o server.o activity.o repo.o
client: client.o repo.o
clean:
	git clean -fdx

testgit: testgit.o repo.o
