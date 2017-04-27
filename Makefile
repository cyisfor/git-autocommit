OPT=-g
LDFLAGS+=$(OPT) -luv -lgit2
CFLAGS+=$(OPT)
all: index_reader server client
server: check.o server.o activity.o repo.o net.o
client: client.o repo.o net.o
clean:
	git clean -fdx

index_reader: index_reader.o repo.o activity.o
