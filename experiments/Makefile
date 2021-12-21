# Makefile v 1.02

SRC = src

CC = gcc
CFLAGS = -std=c17 -O1 -Wall -Werror -pedantic
LFLAGS = -lcrypto -lpthread

all: server serverRR serverRRW

server: $(SRC)/server.c
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

serverRR: $(SRC)/serverRoundRobin.c
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

serverRRW: $(SRC)/serverRoundRobinWeighted.c
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS) 

clean:
	rm -f server serverRR serverRRW
