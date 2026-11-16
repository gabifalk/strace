/*
 * Check JSONL output of directory entries that are not printed in full:
 * the dirp argument of getdents64 without -v (addr_count of entries).
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_getdents64

# include "jsonl.h"

# include <fcntl.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/stat.h>

static const char dname[] = "jsonl-getdents64.dir";

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const exit_side = merged ? "exit" : NULL;
	static char buf[4096];

	(void) rmdir(dname);
	if (mkdir(dname, 0700))
		perror_msg_and_fail("mkdir: %s", dname);
	const int fd = open(dname, O_RDONLY | O_DIRECTORY);
	if (fd < 0)
		perror_msg_and_fail("open: %s", dname);

	long rc = syscall(__NR_getdents64, fd, buf, sizeof(buf));
	int err = errno;

	if (merged)
		jsonl_syscall_merged_open("getdents64", __NR_getdents64);
	else
		jsonl_syscall_open("getdents64", __NR_getdents64, true);
	jsonl_arg_open(0, "fd", merged ? "entry" : NULL);
	jsonl_fd(fd, NULL, false);
	jsonl_arg_close(merged ? "entry" : NULL);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("getdents64", __NR_getdents64, false);
	} else {
		printf(", ");
	}
	jsonl_arg_open(1, "dirp", exit_side);
	printf("\"type\": \"addr_count\", \"raw\": \"%#lx\", \"count\": \"2\","
	       " \"unit\": \"entries\"", (unsigned long) (uintptr_t) buf);
	jsonl_arg_close(exit_side);
	printf(", ");
	jsonl_arg_open(2, "count", exit_side);
	jsonl_unsigned(sizeof(buf));
	jsonl_arg_close(exit_side);
	printf("], ");
	errno = err;
	jsonl_return("unsigned", rc);
	printf("}\n");

	close(fd);
	if (rmdir(dname))
		perror_msg_and_fail("rmdir: %s", dname);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_getdents64")

#endif
