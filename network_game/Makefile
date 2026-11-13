CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic
LDLIBS =
PYTHON = python3
ifeq ($(OS),Windows_NT)
LDLIBS = -lws2_32
endif

all: server client

server: server.c net.c net.h game.c game.h
	$(CC) $(CFLAGS) server.c net.c game.c -o server $(LDLIBS)

client: client.c net.c net.h game.c game.h
	$(CC) $(CFLAGS) client.c net.c game.c -o client $(LDLIBS)

test_game: test_game.c game.c game.h
	$(CC) $(CFLAGS) test_game.c game.c -o test_game

test: all test_game
	./test_game
	$(PYTHON) test_network.py

.PHONY: all test
