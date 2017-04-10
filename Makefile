LDFLAGS+=-luv
CFLAGS+=-g
all: main client
main: check.c main.c
client: client.c
