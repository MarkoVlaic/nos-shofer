CC=gcc
CFLAGS=-Werror -Wall

all: reader writer.o

reader: reader.c
	$(CC) $(CFLAGS) reader.c -o reader

writer: writer.c
	$(CC) $(CFLAGS) writer.c -o writer

clean:
	rm -f reader writer