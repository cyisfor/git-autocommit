LDFLAGS+=-luv -lm
CFLAGS+=-g
all: server client
server: check.c server.c
client: client.c
