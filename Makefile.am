ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -fdiagnostics-color=always $(EVENT_CFLAGS) $(GIT_CFLAGS) -pthread -fPIC

lib_LTLIBRARIES = libautocommit.la

libautocommit_la_SOURCES = \
	src/activity.c \
	src/check.c\
	src/net.c \
	src/repo.c \
	src/hooks.c \
	src/checkpid.c \
	src/eventbase.c \
	src/continuation.c \
	src/note.c

bin_PROGRAMS = server client

server_SOURCES = src/server.c
server_LDADD = libautocommit.la $(EVENT_LIBS) $(GIT_LIBS) $(LIBS)
client_SOURCES = src/client.c 
client_LDADD = libautocommit.la $(EVENT_LIBS) $(GIT_LIBS) $(LIBS)

BUILT_SOURCES = my_cflags.h

# need the source locations, for hooks that want to compile with our headers
# AC_DEFINE fails btw

my_cflags.h: Makefile.am
	echo '#define MY_CFLAGS "-I'$(realpath $(srcdir))'/src" '$(EVENT_CFLAGS) > $@.temp
	echo '#define MY_LDFLAGS "'$(realpath libautocommit.la) $(EVENT_LIBS)'"' >> $@.temp
	mv $@.temp $@

