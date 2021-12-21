# Makefile v 1.02

SRC = src

CC = gcc
CFLAGS = -std=c17 -O1 -Wall -Werror -pedantic
LFLAGS = -lcrypto -lpthread

server: $(SRC)/server.c
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS) 

clean:
	rm -f server
