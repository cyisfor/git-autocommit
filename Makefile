PKG_CONFIG_PATH=/custom/libuv/lib/pkgconfig
export PKG_CONFIG_PATH
OPT=-g -O2
LDFLAGS+=$(OPT) `pkg-config libuv --libs` -lgit2 -ldl
CFLAGS+=$(OPT) `pkg-config libuv --cflags` -fPIC -DSOURCE_LOCATION='"'`pwd`'"'
all: index_reader server client
libautocommit.a: activity.o check.o net.o repo.o hooks.o
	ar crs $@ $^
server: server.o libautocommit.a
client: client.o libautocommit.a
clean:
	git clean -fdx

index_reader: index_reader.o repo.o activity.o
