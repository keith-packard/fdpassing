CFLAGS=-O0 -g

all: fdpassing twowrites xreq

fdpassing: fdpassing.o fdpass.o
	$(CC) $(CFLAGS) -o $@ fdpassing.o fdpass.o

twowrites: twowrites.o fdpass.o
	$(CC) $(CFLAGS) -o $@ twowrites.o fdpass.o

xreq: xreq.o fdpass.o
	$(CC) $(CFLAGS) -o $@ xreq.o fdpass.o
