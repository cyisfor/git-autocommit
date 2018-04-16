ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -fdiagnostics-color=always $(EVENT_CFLAGS) $(GIT_CFLAGS) -pthread

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
