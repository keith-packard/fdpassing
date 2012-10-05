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

void
child(int sock)
{
	int	fd;
	char	buf[16];
	ssize_t	size;

	size = sock_fd_read(sock, buf, sizeof(buf), &fd);
	printf ("read %d\n", size);
	if (fd != -1)
		write(fd, "hello, world\n", 13);
}

void
parent(int sock)
{
	ssize_t	size;
	int	i;
	int	fd;

	fd = 1;
	size = sock_fd_write(sock, "1", 1, 1);
	printf ("wrote %d\n", size);
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
