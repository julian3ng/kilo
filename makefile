CC=clang
CFLAGS=-Wall -Werror -Wextra -pedantic -g -std=c99
EXE=kilo
OBJ=kilo.o
SRC=kilo.c

all: kilo

kilo: $(OBJ)
	$(CC) -g $^ -o $@

%.o: %.c
	$(CC) -c -g $< -o $@

.PHONY: clean
clean:
	rm -rf kilo kilo.o 
