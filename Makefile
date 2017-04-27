OPT=-g
LDFLAGS+=$(OPT) -luv -lgit2
CFLAGS+=$(OPT)
all: index_reader server client
server: server.o activity.o check.o net.o repo.o
client: client.o activity.o check.o net.o repo.o
clean:
	git clean -fdx

index_reader: index_reader.o repo.o activity.o
