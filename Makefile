CFLAGS=-O0 -g

all: fdpassing

fdpassing: fdpassing.o fdpass.o
	$(CC) -o $@ fdpassing.o fdpass.o
