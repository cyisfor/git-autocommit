OPT=-O2
LDFLAGS+=$(OPT) -luv -lgit2
CFLAGS+=$(OPT)
all: index_reader testgit server client
server: check.o server.o activity.o repo.o net.o
client: client.o repo.o net.o
clean:
	git clean -fdx

testgit: testgit.o repo.o check.o activity.o
index_reader: index_reader.o repo.o activity.o
