VPATH = o

P:=libuv libgit2
PKG_CONFIG_PATH:=/custom/libgit2/lib/pkgconfig
export PKG_CONFIG_PATH

CFLAGS+=-ggdb -fdiagnostics-color=always
CFLAGS+=$(patsubst -I%,-isystem%, $(shell pkg-config --cflags $(P))) -I.
CFLAGS+=-DSOURCE_LOCATION='"'`pwd`'"'

LDLIBS+=$(shell pkg-config --libs $(P))

all: server client index_reader

LINK=$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
COMPILE=$(CC) $(CFLAGS) -MMD -MT $@ -c -o $@ $<

# bleah, accumulation side effects...
O=$(patsubst %,o/%.o,$N) \
$(foreach name,$(N),$(eval targets:=$$(targets) $(name)))
S=$(patsubst %,src/%.c,$N)

N=server
server: $O libautocommit.a
	$(LINK)

N=client
client: $O libautocommit.a
	$(LINK)

N=activity check net repo hooks checkpid note
libautocommit.a: $O
	ar crs $@ $^

o/%.o: src/%.c | o
	$(COMPILE)

clean:
	rm -rf o

o:
	mkdir $@

-include $(patsubst %, o/%.d,$(targets))
