/*
 * Check JSONL output of the dup syscall with -y.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"
#include "scno.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool merged;

static void
jsonl_dup(int oldfd, const char *path, bool deleted, long rc)
{
	const char *side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("dup", __NR_dup);
	else
		jsonl_syscall_open("dup", __NR_dup, true);
	jsonl_arg_open(0, "oldfd", side);
	jsonl_fd(oldfd, path, deleted);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("dup", __NR_dup, false);
	}
	printf("], ");
	if (rc < 0) {
		jsonl_return("fd", rc);
	} else {
		printf("\"return\": {");
		jsonl_fd(rc, path, deleted);
		printf("}");
	}
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	skip_if_unavailable("/proc/self/fd/");

	long rc = syscall(__NR_dup, -1);
	jsonl_dup(-1, NULL, false, rc);

	int fd = open("/dev/null", O_RDONLY);
	if (fd < 0)
		perror_msg_and_fail("open: /dev/null");
	rc = syscall(__NR_dup, fd);
	if (rc < 0)
		perror_msg_and_fail("dup");
	jsonl_dup(fd, "/dev/null", false, rc);

	static const char name[] = "jsonl-dup-y.tmp";
	int tfd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (tfd < 0)
		perror_msg_and_fail("open: %s", name);
	char *path = get_fd_path(tfd);
	if (unlink(name))
		perror_msg_and_fail("unlink: %s", name);
	rc = syscall(__NR_dup, tfd);
	if (rc < 0)
		perror_msg_and_fail("dup");
	jsonl_dup(tfd, path, true, rc);
	free(path);

	jsonl_exit_event(0);
	return 0;
}
