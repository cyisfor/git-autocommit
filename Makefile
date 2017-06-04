P=$(shell pkg-config libuv $1)
OPT=-g -O2
LDLIBS+=$(call P,--libs) -lgit2 -ldl 
LDFLAGS+=$(OPT) -rdynamic -pthread
# -rdynamic makes things like checkpid() available to hooks, instead of
# undefined symbol: checkpid
CFLAGS+=$(OPT) $(call P,--cflags) -fPIC -DSOURCE_LOCATION='"'`pwd`'"'
all: index_reader server client
libautocommit.a: activity.o check.o net.o repo.o hooks.o checkpid.o
	ar crs $@ $^
server: server.o libautocommit.a
client: client.o libautocommit.a
clean:
	git clean -fdx

index_reader: index_reader.o repo.o activity.o
