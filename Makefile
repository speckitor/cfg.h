CC=gcc

example: example.c
	$(CC) -o example example.c -Wall -Wextra
