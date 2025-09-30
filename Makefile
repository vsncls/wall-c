CC = gcc
CFLAGS = -Wall -Werror -std=c99

all: wall-c

wall-c: main.c
	$(CC) $(CFLAGS) -o wall-c main.c

clean:
	rm -f wall-c
