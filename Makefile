CFLAGS=-O0 -g

all: fdpassing multiwrite lostfd zerowrite xreq

fdpassing: fdpassing.o fdpass.o
	$(CC) $(CFLAGS) -o $@ fdpassing.o fdpass.o

multiwrite: multiwrite.o fdpass.o
	$(CC) $(CFLAGS) -o $@ multiwrite.o fdpass.o

lostfd: lostfd.o fdpass.o
	$(CC) $(CFLAGS) -o $@ lostfd.o fdpass.o

zerowrite: zerowrite.o fdpass.o
	$(CC) $(CFLAGS) -o $@ zerowrite.o fdpass.o

xreq: xreq.o fdpass.o
	$(CC) $(CFLAGS) -o $@ xreq.o fdpass.o
