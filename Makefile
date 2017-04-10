LDFLAGS+=-luv
CFLAGS+=-g
all: main client
server: check.c server.c
client: client.c
