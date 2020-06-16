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

#define MAX_FDS		128

struct fd_pass {
	struct cmsghdr	cmsghdr;
	int		fd[MAX_FDS];
};

ssize_t
sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd, int *nfdp)
{
	ssize_t		size;

	printf ("sock_fd_read\n");
	if (fd) {
		struct msghdr	msg;
		struct iovec	iov;
		struct fd_pass	pass;
		int		nfd_passed, nfd;
		int		i;
		int		*fd_passed;
		int		type;
		int		length;

		iov.iov_base = buf;
		iov.iov_len = bufsize;

		length = sizeof( int );
		getsockopt (sock, SOL_SOCKET, SO_TYPE, &type, &length);

		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = &pass;
		msg.msg_controllen = sizeof pass;
		size = recvmsg (sock, &msg, 0);
		if (size < 0) {
			perror ("recvmsg");
			exit(1);
		}
		if ((size > 0 || type == SOCK_SEQPACKET) && pass.cmsghdr.cmsg_len > sizeof (struct cmsghdr)) {
			if ((msg.msg_flags & MSG_TRUNC) ||
			    (msg.msg_flags & MSG_CTRUNC)) {
				fprintf (stderr, "control message truncated");
				exit(1);
			}
			if (pass.cmsghdr.cmsg_level != SOL_SOCKET) {
				fprintf (stderr, "invalid cmsg_level %d\n",
					 pass.cmsghdr.cmsg_level);
				exit(1);
			}
			if (pass.cmsghdr.cmsg_type != SCM_RIGHTS) {
				fprintf (stderr, "invalid cmsg_type %d\n",
					 pass.cmsghdr.cmsg_type);
				exit(1);
			}
			nfd_passed = (pass.cmsghdr.cmsg_len - sizeof (struct cmsghdr)) / sizeof (int);
			fd_passed = (int *) CMSG_DATA(&pass.cmsghdr);

			nfd = *nfdp;
			if (nfd > nfd_passed)
				nfd = nfd_passed;

			memcpy(fd, fd_passed, nfd * sizeof (int));
			for (i = 0; i < nfd; i++)
				printf ("received fd %d\n", fd[i]);
			for (i = nfd; i < nfd_passed; i++) {
				printf ("dropping fd %d\n", fd_passed[i]);
				close(fd_passed[i]);
			}
			*nfdp = nfd;
		} else
			*nfdp = 0;
	} else {
		size = read (sock, buf, bufsize);
		if (size < 0) {
			perror("read");
			exit(1);
		}
	}
	return size;
}

ssize_t
sock_fd_write(int sock, void *buf, ssize_t buflen, int *fd, int nfd)
{
	ssize_t		size;
	struct msghdr	msg;
	struct iovec	iov;
	struct fd_pass	pass;
	int		i;

	iov.iov_base = buf;
	iov.iov_len = buflen;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	if (buflen) {
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
	} else {
		msg.msg_iov = NULL;
		msg.msg_iovlen = 0;
	}

	if (nfd) {
		if (nfd > MAX_FDS) {
			printf ("passing too many fds\n");
			exit(1);
		}

		msg.msg_control = &pass;
		msg.msg_controllen = sizeof (struct cmsghdr) + nfd * sizeof (int);

		pass.cmsghdr.cmsg_len = msg.msg_controllen;
		pass.cmsghdr.cmsg_level = SOL_SOCKET;
		pass.cmsghdr.cmsg_type = SCM_RIGHTS;

		memcpy(&pass.fd, fd, nfd * sizeof (int));
		for (i = 0; i < nfd; i++)
			printf ("passing fd %d\n", pass.fd[i]);
	} else {
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		printf ("not passing fd\n");
	}

	size = sendmsg(sock, &msg, 0);

	if (size < 0)
		perror ("sendmsg");
	return size;
}
