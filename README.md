## FD passing for DRI.Next

Using the DMA-BUF interfaces to pass DRI objects between the client
and server, as discussed in my previous blog posting on [[DRI-Next]],
requires that we successfully pass file descriptors over the X
protocol socket.

Rumor has it that this has been tried and found to be difficult, and
so I decided to do a bit of experimentation to see how this could be
made to work within the existing X implementation.

(All of the examples shown here are licensed under the GPL, version 2
and are available from git://keithp.com/git/fdpassing)

### Basics of FD passing

The kernel internals that support FD passing are actually quite simple
-- POSIX already require that two processes be able to share the same
underlying reference to a file because of the semantics of the fork(2)
call. Adding some ability to share arbitrary file descriptors between
two processes then is far more about how you ask the kernel than the
actual file descriptor sharing operation.

In Linux, file descriptors can be passed through local network
sockets. The sender constructs a mystic-looking sendmsg(2) call,
placing the file descriptor in the control field of that
operation. The kernel pulls the file descriptor out of the control
field, allocates a file descriptor in the target process which
references the same file object and then sticks the file descriptor in
a queue for the receiving process to fetch.

The receiver then constructs a matching call to recvmsg that provides
a place for the kernel to stick the new file descriptor.

### A helper API for testing

I first write a stand-alone program that created a socketpair, forked
and then passed an fd from the parent to the child. Once that was
working, I decided that some short helper functions would make further
testing a whole lot easier.

Here's a function that writes some data and an optional file
descriptor:

	ssize_t
	sock_fd_write(int sock, void *buf, ssize_t buflen, int fd)
	{
		ssize_t		size;
		struct msghdr	msg;
		struct iovec	iov;
		union {
			struct cmsghdr	cmsghdr;
			char		control[CMSG_SPACE(sizeof (int))];
		} cmsgu;
		struct cmsghdr	*cmsg;

		iov.iov_base = buf;
		iov.iov_len = buflen;

		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;

		if (fd != -1) {
			msg.msg_control = cmsgu.control;
			msg.msg_controllen = sizeof(cmsgu.control);

			cmsg = CMSG_FIRSTHDR(&msg);
			cmsg->cmsg_len = CMSG_LEN(sizeof (int));
			cmsg->cmsg_level = SOL_SOCKET;
			cmsg->cmsg_type = SCM_RIGHTS;

			printf ("passing fd %d\n", fd);
			*((int *) CMSG_DATA(cmsg)) = fd;
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

And here's the matching receiver function:

	ssize_t
	sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd)
	{
		ssize_t		size;

		if (fd) {
			struct msghdr	msg;
			struct iovec	iov;
			union {
				struct cmsghdr	cmsghdr;
				char		control[CMSG_SPACE(sizeof (int))];
			} cmsgu;
			struct cmsghdr	*cmsg;

			iov.iov_base = buf;
			iov.iov_len = bufsize;

			msg.msg_name = NULL;
			msg.msg_namelen = 0;
			msg.msg_iov = &iov;
			msg.msg_iovlen = 1;
			msg.msg_control = cmsgu.control;
			msg.msg_controllen = sizeof(cmsgu.control);
			size = recvmsg (sock, &msg, 0);
			if (size < 0) {
				perror ("recvmsg");
				exit(1);
			}
			cmsg = CMSG_FIRSTHDR(&msg);
			if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
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

				*fd = *((int *) CMSG_DATA(cmsg));
				printf ("received fd %d\n", *fd);
			} else
				*fd = -1;
		} else {
			size = read (sock, buf, bufsize);
			if (size < 0) {
				perror("read");
				exit(1);
			}
		}
		return size;
	}

With these two functions, I rewrote the simple example as follows:

	void
	child(int sock)
	{
		int	fd;
		char	buf[16];
		ssize_t	size;

		sleep(1);
		for (;;) {
			size = sock_fd_read(sock, buf, sizeof(buf), &fd);
			if (size <= 0)
				break;
			printf ("read %d\n", size);
			if (fd != -1) {
				write(fd, "hello, world\n", 13);
				close(fd);
			}
		}
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

### Experimenting with multiple writes

I wanted to know what would happen if multiple writes were made, some
with file descriptors and some without. So I changed the simple example
parent function to look like:

	void
	parent(int sock)
	{
		ssize_t	size;
		int	i;
		int	fd;

		fd = 1;
		size = sock_fd_write(sock, "1", 1, -1);
		printf ("wrote %d without fd\n", size);
		size = sock_fd_write(sock, "1", 1, 1);
		printf ("wrote %d with fd\n", size);
		size = sock_fd_write(sock, "1", 1, -1);
		printf ("wrote %d without fd\n", size);
	}

When run, this demonstrates that the reader gets two bytes in the
first read along with a file descriptor followed by one byte in a
second read, without a file descriptor. This demonstrates that
a file descriptor message forms a barrier within the socket; multiple
messages will be merged together, but not past a message containing a
file descriptor.

### Reading without accepting a file descriptor

What happens when the reader isn't expecting a file descriptor? Does
it just get lost? Does the reader not get the message until it asks
for the file descriptor? What about the boundary issue described above?

Here's my test case:

	void
	child(int sock)
	{
		int	fd;
		char	buf[16];
		ssize_t	size;

		sleep(1);
		size = sock_fd_read(sock, buf, sizeof(buf), NULL);
		if (size <= 0)
			return;
		printf ("read %d\n", size);
		size = sock_fd_read(sock, buf, sizeof(buf), &fd);
		if (size <= 0)
			return;
		printf ("read %d\n", size);
		if (fd != -1) {
			write(fd, "hello, world\n", 13);
			close(fd);
		}
	}

	void
	parent(int sock)
	{
		ssize_t	size;
		int	i;
		int	fd;

		fd = 1;
		size = sock_fd_write(sock, "1", 1, 1);
		printf ("wrote %d without fd\n", size);
		size = sock_fd_write(sock, "1", 1, 2);
		printf ("wrote %d with fd\n", size);
	}

This shows that the first passed file descriptor is picked up by the
first sock_fd_read call, but the file descriptor is closed. The second
file descriptor passed is picked up by the second sock_fd_read call.

### Zero-length writes

Can a file descriptor be passed without sending any data?

	void
	parent(int sock)
	{
		ssize_t	size;
		int	i;
		int	fd;

		fd = 1;
		size = sock_fd_write(sock, "1", 1, -1);
		printf ("wrote %d without fd\n", size);
		size = sock_fd_write(sock, NULL, 0, 1);
		printf ("wrote %d with fd\n", size);
		size = sock_fd_write(sock, "1", 1, -1);
		printf ("wrote %d without fd\n", size);
	}

And the answer is clearly "no" -- the file descriptor is not passed
when no data are included in the write.

(update, 2019-5-8 from Nicholas Rishel)

This is true for SOCK_STREAM sockets, but for SOCK_SEQPACKET sockets,
you *can* do zero-length writes and pass an fd.

### A summary of results


 1. read and recvmsg don't merge data across a file descriptor message boundary.

 2. failing to accept an fd in the receiver results in the fd being
    closed by the kernel.

 3. a file descriptor must be accompanied by some data if sent via stream.

## Make X pass file descriptors

I'd like to get X to pass a file descriptor without completely
rewriting the internals of both the library and the X server. Ideally,
without making any changes to the existing code paths for regular
request processing at all.

On the sending side, this seems pretty straightforward -- we just need
to get the X connection file descriptor and call sendmsg directly,
passing the desired file descriptor along. In XCB, this could be done
by using the xcb_take_socket interface to temporarily hijack the
protocol as Xlib does.

It's the receiving side where things are messier. Because a bare read
will discard any delivered file descriptor, we must make sure to use
recvmsg whenever we want to actually capture the file descriptor.

### Kludge X server fd receiving

Because a passed fd creates a barrier in the bytestream, when the X
server reads requests from a client, the read will stop sending data
after the message with the file descriptor is consumed.

Of course, this process consumes the passed file descriptor, and if
that call isn't made with recvmsg set up to receive it, the fd will be
lost.

As a simple kludge, if we pass a meaningless fd with the X request and
then the 'real' fd with a following XNoOperation request, the existing
request reading code will get the request, discard the meaningless fd
and then stop reading at that point due to the barrier. Once into the
request processing code, recvmsg can be called to get the real file
descriptor and the associated XNoOperation request.

I wrote a test for this that demonstrates how this works:

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

### Fixing XCB to receive file descriptors

Multiple threads may be trying to get replies and events back from the
X server at the same time, which means the kludge of having the real
fd follow the message will likely lead to the wrong thread getting the
file descriptor.

Instead, I suspect the best plan will be to fix XCB to internally
capture passed file descriptors and save them with the associated
reply. Because the file descriptor message will form a barrier in the
read stream, xcb can associate any received file descriptor with the
last reply in the read data. The X server would then send the reply
with an explicit sendmsg call to pass both reply and file descriptor
together.

## Next steps

The next thing to do is code up a simple fd passing extension and try
to get it working, passing descriptors back and forth to the X
server. Once that works, design of the rest of the DRM-Next extension
should be pretty straightforward.
