CC=gcc
CFLAGS=-Wall -Werror
ifdef ${CROSS_COMPILE}
	CC=${CROSS_COMPILE}gcc
endif

all:	aesdsocket.o
	$(CC) -o aesdsocket aesdsocket.o $(CFLAGS)

clean:
	rm -rf *.o


