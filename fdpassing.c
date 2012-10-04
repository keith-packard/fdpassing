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

#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

void
child(int sock)
{
	ssize_t		size;
	struct msghdr	msg;
	struct iovec	iov;
	char		buf[16];
	union {
		struct cmsghdr	cmsghdr;
		char		control[CMSG_SPACE(sizeof (int))];
	} cmsgu;
	struct cmsghdr	*cmsg;

	int		fd;

	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsgu.control;
	msg.msg_controllen = sizeof(cmsgu.control);

	size = recvmsg (sock, &msg, 0);
	if (size < 0) {
		perror("recvmsg");
		exit(1);
	}

	cmsg = CMSG_FIRSTHDR(&msg);
	if (cmsg &&
	    cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
		if (cmsg->cmsg_level != SOL_SOCKET) {
			fprintf (stderr, "invalid cmsg_level %d\n",
				 cmsg->cmsg_level);
			exit(1);
		}
		if (cmsg->cmsg_type != SCM_RIGHTS) {
			fprintf (stderr, "invalid cmsg_type %d\n",
				 cmsg->cmsg_type);
			exit(1);
		}

		fd = *((int *) CMSG_DATA(cmsg));
		printf ("received fd %d\n", fd);
		write(fd, "hello, world\n", 13);
	} else {
		fd = -1;
	}
	exit(0);
}

void
parent(int sock)
{
	ssize_t		size;
	struct msghdr	msg;
	struct iovec	iov;
	char		buf[16];
	union {
		struct cmsghdr	cmsghdr;
		char		control[CMSG_SPACE(sizeof (int))];
	} cmsgu;
	struct cmsghdr	*cmsg;

	int		fd;

	iov.iov_base = "1";
	iov.iov_len = 1;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsgu.control;
	msg.msg_controllen = sizeof(cmsgu.control);

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_len = CMSG_LEN(sizeof (int));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;

	fd = creat("test-file", 0666);
	printf ("passing fd %d\n", fd);
	*((int *) CMSG_DATA(cmsg)) = fd;

	size = sendmsg(sock, &msg, 0);

	if (size < 0)
		perror ("sendmsg");
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
		child(sv[1]);
		break;
	case -1:
		perror("fork");
		exit(1);
	default:
		parent(sv[0]);
		sleep(1);
		kill(pid, SIGTERM);
		break;
	}
}
