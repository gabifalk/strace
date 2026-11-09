/*
 * Check JSONL output of the pidfd_getfd syscall with -yy.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_pidfd_getfd && defined __NR_pidfd_open

# include "jsonl.h"

# include <fcntl.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

static bool merged;

static void
jsonl_open(void)
{
	if (merged)
		jsonl_syscall_merged_open("pidfd_getfd", __NR_pidfd_getfd);
	else
		jsonl_syscall_open("pidfd_getfd", __NR_pidfd_getfd, true);
}

static void
jsonl_flags(const char *side)
{
	printf(", ");
	jsonl_arg_open(2, "flags", side);
	printf("\"type\": \"hex\", \"raw\": \"0\"");
	jsonl_arg_close(side);
}

static void
jsonl_close(void)
{
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("pidfd_getfd", __NR_pidfd_getfd, false);
	}
	printf("], ");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *side = merged ? "entry" : NULL;
	skip_if_unavailable("/proc/self/fd/");

	long rc = syscall(__NR_pidfd_getfd, -1, -1, 0);
	jsonl_open();
	jsonl_arg_open(0, "pidfd", side);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "targetfd", side);
	printf("\"type\": \"decimal\", \"raw\": \"-1\"");
	jsonl_arg_close(side);
	jsonl_flags(side);
	jsonl_close();
	jsonl_return("fd", rc);
	printf("}\n");

	int pidfd = syscall(__NR_pidfd_open, getpid(), 0);
	if (pidfd < 0)
		perror_msg_and_skip("pidfd_open");
	int fd = open("/dev/null", O_RDONLY);
	if (fd < 0)
		perror_msg_and_fail("open: /dev/null");
	struct stat st;
	if (fstat(fd, &st))
		perror_msg_and_fail("fstat");
	rc = syscall(__NR_pidfd_getfd, pidfd, fd, 0);
	if (rc < 0)
		perror_msg_and_skip("pidfd_getfd");
	jsonl_open();
	jsonl_arg_open(0, "pidfd", side);
	printf("\"type\": \"fd\", \"raw\": \"%d\","
	       " \"fd_info\": {\"type\": \"pidfd\", \"pid\": \"%d\"}",
	       pidfd, (int) getpid());
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "targetfd", side);
	jsonl_fd_dev(fd, "/dev/null", &st);
	jsonl_arg_close(side);
	jsonl_flags(side);
	jsonl_close();
	printf("\"return\": {");
	jsonl_fd_dev(rc, "/dev/null", &st);
	printf("}}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_pidfd_getfd && __NR_pidfd_open")

#endif
