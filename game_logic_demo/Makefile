CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic

ipis_logic: main.c game.c game.h
	$(CC) $(CFLAGS) main.c game.c -o ipis_logic

test_game: test_game.c game.c game.h
	$(CC) $(CFLAGS) test_game.c game.c -o test_game

test: test_game
	./test_game

.PHONY: test
