/*
 * Check JSONL output of the file status flags returned by fcntl F_GETFL.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_fcntl

# include "jsonl.h"

# include <fcntl.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "entry" : NULL;

	int fds[2];
	if (pipe(fds))
		perror_msg_and_fail("pipe");
	long rc;

	rc = syscall(__NR_fcntl, fds[1], F_GETFL, 0);
	if (merged)
		jsonl_syscall_merged_open("fcntl", __NR_fcntl);
	else
		jsonl_syscall_open("fcntl", __NR_fcntl, true);
	jsonl_arg_open(0, "fd", side);
	jsonl_fd(fds[1], NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "op", side);
	jsonl_const(F_GETFL, true, "F_GETFL", NULL);
	jsonl_arg_close(side);
	if (rc != O_WRONLY)
		error_msg_and_skip("F_GETFL: unexpected flags %#lx", rc);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("fcntl", __NR_fcntl, false);
	}
	printf("], \"return\": {");
	jsonl_const(O_WRONLY, true, "O_WRONLY", NULL);
	printf("}}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_fcntl")

#endif
