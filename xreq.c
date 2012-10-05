/*
 * Copyright © 2012 Keith Packard <keithp@keithp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include <stdint.h>
#include "fdpass.h"

static void
child(int sock)
{
	uint8_t	xreq[1024];
	uint8_t	xnop[4];
	uint8_t	req;
	int	i, reqlen;
	ssize_t	size, fdsize;
	int	fd = -1, *fdp;
	int	j;

	sleep (1);
	for (j = 0;; j++) {
		size = sock_fd_read(sock, xreq, sizeof (xreq), NULL);
		printf ("got %d\n", size);
		if (size == 0)
			break;
		i = 0;
		while (i < size) {
			req = xreq[i];
			reqlen = xreq[i+1];
			i += reqlen;
			switch (req) {
			case 0:
				break;
			case 1:
				if (i != size) {
					fprintf (stderr, "Got fd req, but not at end of input %d < %d\n",
						 i, size);
				}
				fdsize = sock_fd_read(sock, xnop, sizeof (xnop), &fd);
				if (fd == -1) {
					fprintf (stderr, "no fd received\n");
				} else {
					FILE	*f = fdopen (fd, "w");
					fprintf(f, "hello %d\n", j);
					fflush(f);
					fclose(f);
					close(fd);
					fd = -1;
				}
				break;
			case 2:
				fprintf (stderr, "Unexpected FD passing req\n");
				break;
			}
		}
	}
}

int
tmp_file(int j) {
	char	name[64];

	sprintf (name, "tmp-file-%d", j);
	return creat(name, 0666);
}

static void
parent(int sock)
{
	uint8_t	xreq[32];
	uint8_t	xnop[4];
	int	i, j;
	int	fd;

	for (j = 0; j < 4; j++) {
		/* Write a bunch of regular requests */
		for (i = 0; i < 8; i++) {
			xreq[0] = 0;
			xreq[1] = sizeof (xreq);
			sock_fd_write(sock, xreq, sizeof (xreq), -1);
		}

		/* Write our 'pass an fd' request with a 'useless' FD to block the receiver */
		xreq[0] = 1;
		xreq[1] = sizeof(xreq);
		sock_fd_write(sock, xreq, sizeof (xreq), 1);

		/* Pass an fd */
		xnop[0] = 2;
		xnop[1] = sizeof (xnop);
		fd = tmp_file(j);
		sock_fd_write(sock, xnop, sizeof (xnop), fd);
		close(fd);
	}
}

int
main(int argc, char **argv)
{
	int	sv[2];
	int	pid;

	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sv) < 0) {
		perror("socketpair");
		exit(1);
	}
	switch ((pid = fork())) {
	case 0:
		close(sv[0]);
		child(sv[1]);
		break;
	case -1:
		perror("fork");
		exit(1);
	default:
		close(sv[1]);
		parent(sv[0]);
		break;
	}
	return 0;
}
