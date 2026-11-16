/*
 * Check JSONL output of a syscall whose exit is decoded after another
 * traced process made a syscall of its own.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_accept && defined __NR_getresuid

# include "jsonl.h"

# include <fcntl.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <sys/socket.h>
# include <sys/un.h>
# include <sys/wait.h>

# if defined __NR_getresuid32 && __NR_getresuid != __NR_getresuid32
#  define UGID_TYPE	short
# else
#  define UGID_TYPE	int
# endif

# define TEST_SOCKET "jsonl-accept-f.socket"

static bool merged;

static void
jsonl_uid(unsigned int index, const char *name, unsigned val)
{
	const char *side = merged ? "exit" : NULL;

	jsonl_arg_open(index, name, side);
	printf("\"type\": \"uid\", \"raw\": \"%u\", \"indirect\": true",
	       val);
	jsonl_arg_close(side);
}

static void
jsonl_getresuid(pid_t pid, unsigned r, unsigned e, unsigned s)
{
	if (merged) {
		jsonl_syscall_merged_open_pid(pid, "getresuid", __NR_getresuid);
	} else {
		jsonl_syscall_open_pid(pid, "getresuid", __NR_getresuid, true);
		printf("]}\n");
		jsonl_syscall_open_pid(pid, "getresuid", __NR_getresuid, false);
	}
	jsonl_uid(0, "ruid", r);
	printf(", ");
	jsonl_uid(1, "euid", e);
	printf(", ");
	jsonl_uid(2, "suid", s);
	printf("], \"return\": {\"type\": \"unsigned\", \"raw\": \"0\"}}\n");
}

static void
jsonl_addrlen(socklen_t len, const char *extra)
{
	printf("\"type\": \"decimal\", \"raw\": \"%u\", \"indirect\": true%s",
	       (unsigned int) len, extra);
}

static void
jsonl_accept_entry(pid_t pid, int lfd)
{
	const char *side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open_pid(pid, "accept", __NR_accept);
	else
		jsonl_syscall_open_pid(pid, "accept", __NR_accept, true);
	jsonl_arg_open(0, "sockfd", side);
	printf("\"type\": \"fd\", \"raw\": \"%d\"", lfd);
	jsonl_arg_close(side);
	if (!merged) {
		printf(", ");
		jsonl_arg_open(2, "addrlen", NULL);
		jsonl_addrlen(sizeof(struct sockaddr_un), ", \"inout\": true");
		jsonl_arg_close(NULL);
		printf("]}\n");
	}
}

static void
jsonl_accept_exit(pid_t pid, int afd)
{
	const char *side = merged ? "exit" : NULL;

	if (merged)
		printf(", ");
	else
		jsonl_syscall_open_pid(pid, "accept", __NR_accept, false);
	jsonl_arg_open(1, "addr", side);
	printf("\"type\": \"sockaddr\", \"family\": {\"type\": \"const\","
	       " \"raw\": \"0x1\", \"sym\": \"AF_UNIX\"}");
	jsonl_arg_close(side);
	printf(", ");
	if (merged) {
		jsonl_arg_open(2, "addrlen", "entry");
		jsonl_addrlen(sizeof(struct sockaddr_un), ", \"inout\": true");
		printf("}, \"exit\": {");
	} else {
		jsonl_arg_open(2, "addrlen", NULL);
	}
	jsonl_addrlen(sizeof(sa_family_t), ", \"changed\": true");
	jsonl_arg_close(side);
	printf("], \"return\": {\"type\": \"fd\", \"raw\": \"%d\"}}\n",
	       afd);
}

/* Wait until the child sleeps inside the accept syscall. */
static void
wait_for_accept(pid_t pid)
{
	char path[64];
	char buf[256];

	snprintf(path, sizeof(path), "/proc/%d/syscall", (int) pid);
	for (unsigned int i = 0; i < 10000; i++) {
		int fd = open(path, O_RDONLY);
		if (fd < 0)
			perror_msg_and_skip("open: %s", path);
		ssize_t n = read(fd, buf, sizeof(buf) - 1);
		close(fd);
		if (n < 0)
			perror_msg_and_skip("read: %s", path);
		buf[n] = '\0';
		if (strtol(buf, NULL, 10) == __NR_accept) {
			snprintf(path, sizeof(path), "/proc/%d/stat",
				 (int) pid);
			fd = open(path, O_RDONLY);
			if (fd < 0)
				perror_msg_and_skip("open: %s", path);
			n = read(fd, buf, sizeof(buf) - 1);
			close(fd);
			if (n < 0)
				perror_msg_and_skip("read: %s", path);
			buf[n] = '\0';
			const char *state = strrchr(buf, ')');
			if (state && state[1] == ' ' && state[2] == 'S')
				return;
			snprintf(path, sizeof(path), "/proc/%d/syscall",
				 (int) pid);
		}
		usleep(1000);
	}
	error_msg_and_skip("child did not block in accept");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	int lfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (lfd < 0)
		perror_msg_and_skip("socket");

	(void) unlink(TEST_SOCKET);

	const struct sockaddr_un un = {
		.sun_family = AF_UNIX,
		.sun_path = TEST_SOCKET
	};

	if (bind(lfd, (const void *) &un, sizeof(un)))
		perror_msg_and_skip("bind");
	if (listen(lfd, 1))
		perror_msg_and_skip("listen");

	int fds[2];
	if (pipe(fds))
		perror_msg_and_fail("pipe");

	pid_t pid = fork();
	if (pid < 0)
		perror_msg_and_fail("fork");

	if (!pid) {
		close(fds[0]);
		TAIL_ALLOC_OBJECT_CONST_PTR(socklen_t, plen);
		*plen = sizeof(struct sockaddr_un);
		struct sockaddr_un *addr = tail_alloc(*plen);
		int afd = syscall(__NR_accept, lfd, addr, plen);
		if (afd < 0)
			_exit(2);
		if (write(fds[1], &afd, sizeof(afd)) != sizeof(afd))
			_exit(3);
		_exit(0);
	}

	close(fds[1]);
	wait_for_accept(pid);

	TAIL_ALLOC_OBJECT_CONST_PTR(unsigned UGID_TYPE, r);
	TAIL_ALLOC_OBJECT_CONST_PTR(unsigned UGID_TYPE, e);
	TAIL_ALLOC_OBJECT_CONST_PTR(unsigned UGID_TYPE, s);
	if (syscall(__NR_getresuid, r, e, s))
		perror_msg_and_fail("getresuid");

	int cfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (cfd < 0)
		perror_msg_and_fail("socket");
	if (connect(cfd, (const void *) &un, sizeof(un)))
		perror_msg_and_fail("connect");

	int afd;
	if (read(fds[0], &afd, sizeof(afd)) != sizeof(afd))
		perror_msg_and_fail("read");

	int status;
	if (wait(&status) != pid || status)
		error_msg_and_fail("child failed");

	(void) unlink(TEST_SOCKET);

	const pid_t ppid = getpid();
	if (merged) {
		jsonl_getresuid(ppid, *r, *e, *s);
		jsonl_accept_entry(pid, lfd);
		jsonl_accept_exit(pid, afd);
	} else {
		jsonl_accept_entry(pid, lfd);
		jsonl_getresuid(ppid, *r, *e, *s);
		jsonl_accept_exit(pid, afd);
	}
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_accept && __NR_getresuid")

#endif
