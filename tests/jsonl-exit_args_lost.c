/*
 * Check JSONL output of a syscall exit whose exit-time arguments are lost:
 * the leader blocked in readv is superseded by an execve of another thread.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_readv

# include "jsonl.h"

# include <errno.h>
# include <pthread.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/uio.h>

static bool merged;
static const char *side;
static int fds[2];

static void *
thread(void *arg)
{
	usleep(100000);

	if (merged)
		jsonl_syscall_merged_open("readv", __NR_readv);
	else
		jsonl_syscall_open("readv", __NR_readv, true);
	jsonl_arg_open(0, "fd", side);
	jsonl_fd(fds[0], NULL, false);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("readv", __NR_readv, false);
	}
	printf("], \"exit_args_lost\": true,"
	       " \"return\": {\"type\": \"unavailable\"}}\n");

	char *argv[] = { ((char **) arg)[0], (char *) "0", NULL };
	execve(argv[0], argv, NULL);
	perror_msg_and_fail("execve");
}

int
main(int argc, char *argv[])
{
	if (argc > 1 && !strcmp(argv[1], "0"))
		return 0;

	setvbuf(stdout, NULL, _IONBF, 0);
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	if (pipe(fds))
		perror_msg_and_fail("pipe");

	pthread_t t;
	errno = pthread_create(&t, NULL, thread, argv);
	if (errno)
		perror_msg_and_fail("pthread_create");

	char buf[1];
	struct iovec iov = { .iov_base = buf, .iov_len = sizeof(buf) };
	syscall(__NR_readv, fds[0], &iov, 1);

	return 1;
}

#else

SKIP_MAIN_UNDEFINED("__NR_readv")

#endif
