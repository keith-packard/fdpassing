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

#include "fdpass.h"

char *
itoa(int i, char *buf)
{
	buf += 10;
	*--buf = '\0';
	do {
		*--buf = '0' + i % 10;
	} while ((i /= 10));
	return buf;
}

void
handler(int i)
{
	char	buf[12];
	char	*b = itoa(i, buf);
	
	write (2, "signal ", 7);
	write (2, b, strlen(b));
	write (2, "\n", 1);
	_exit(1);
}

void
child(int sock)
{
	int	fd, *fdp;
	char	buf[16];
	ssize_t	size;
	int	i = 0;

	signal(SIGSEGV, handler);
	sleep(1);
	for (;;) {
		if (!(i & 1))
			fdp = NULL;
		else
			fdp = &fd;
		size = sock_fd_read(sock, buf, sizeof(buf), fdp);
		if (size == 0)
			break;
		printf ("read %d\n", size);
		if (fdp && fd != -1)
			write(fd, "hello, world\n", 13);
		i++;
	}
	printf ("child done\n");
}

void
parent(int sock)
{
	ssize_t	size;
	int	i;
	int	fd;

	for (i = 0; i < 8; i++) {
		if ((i & 1))
			fd = -1;
		else
			fd = 1;
		size = sock_fd_write(sock, "1", 1, fd);
		printf ("wrote %d\n", size);
	}
	close(sock);
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
