ifeq ($(CC),)
	CC = $(CROSS_COMPILE)gcc
endif
ifeq ($(CFLAGS),)
	CFLAGS=-Wall -Werror
endif
ifeq ($(LDFLAGS),)
	LDFLAGS = -lpthread -lrt
endif

all:	aesdsocket.o
	$(CC) -o aesdsocket aesdsocket.o $(CFLAGS) -lpthread -lrt

clean:
	rm -rf *.o


