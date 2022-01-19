CC = gcc
CFLAGS =-Wall -Werror

ifdef CROSS_COMPILE
	CC = $(CROSS_COMPILE)gcc
endif

writer: writer.o
	$(CC) -o writer writer.o

clean:
	rm -f *.o
