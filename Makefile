CFLAGS=-O0 -g

fdpassing: fdpassing.o
	$(CC) -o $@ fdpassing.o