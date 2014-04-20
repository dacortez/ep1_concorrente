CFLAGS = -Wall -ansi -pedantic -lpthread
SOURCE = ep1.c
CC = gcc

ep1: $(SOURCE)
	$(CC) $(SOURCE) -o ep1 $(CFLAGS)

.PHONY: clean
clean:
	rm -rf ep1 *~
