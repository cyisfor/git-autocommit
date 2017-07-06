P=$(shell PKG_CONFIG_PATH=libuv pkg-config libuv $1)
OPT=-g
LDLIBS+=-lgit2 -ldl
#LDLIBS+=libuv/.libs/libuv.a #-lpthread
LDFLAGS+=$(OPT) -rdynamic -pthread
# -rdynamic makes things like checkpid() available to hooks, instead of
# undefined symbol: checkpid
CFLAGS+=$(OPT) -Ilibuv/include/ -fPIC -DSOURCE_LOCATION='"'`pwd`'"'
all: index_reader server client
libautocommit.a: activity.o check.o net.o repo.o hooks.o checkpid.o
	ar crs $@ $^
server: server.o libautocommit.a libuv/.libs/libuv.a
client: client.o libautocommit.a libuv/.libs/libuv.a
clean:
	git clean -fdx

index_reader: index_reader.o repo.o activity.o


libuv/.libs/libuv.a: libuv/Makefile
	$(MAKE) -C libuv

libuv/Makefile: libuv/configure
	cd libuv; ./configure --disable-shared --enable-static

libuv/configure: libuv/configure.ac
	cd libuv; sh autogen.sh

libuv/configure.ac:
	git submodule add https://github.com/joyent/libuv.git 
