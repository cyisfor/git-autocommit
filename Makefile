P=$(shell env PKG_CONFIG_PATH=/custom/libuv/lib/pkgconfig pkg-config libuv $1)
OPT=-g -O2
LDFLAGS+=$(OPT) $(call P,--libs) -lgit2 -ldl
CFLAGS+=$(OPT) $(call P,--cflags) -fPIC -DSOURCE_LOCATION='"'`pwd`'"'
all: index_reader server client
libautocommit.a: activity.o check.o net.o repo.o hooks.o checkpid.o
	ar crs $@ $^
server: server.o libautocommit.a
client: client.o libautocommit.a
clean:
	git clean -fdx

index_reader: index_reader.o repo.o activity.o
