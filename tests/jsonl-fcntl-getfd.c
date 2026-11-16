/*
 * Check JSONL output of the descriptor flags returned by fcntl F_GETFD.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_fcntl && defined __NR_memfd_create

# include "jsonl.h"

# include <fcntl.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <linux/memfd.h>

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "entry" : NULL;

	const int fd = syscall(__NR_memfd_create, "x",
			       MFD_CLOEXEC | MFD_ALLOW_SEALING);
	if (fd < 0)
		perror_msg_and_skip("memfd_create");
	long rc;

	rc = syscall(__NR_fcntl, fd, F_GETFD, 0);
	if (merged)
		jsonl_syscall_merged_open("fcntl", __NR_fcntl);
	else
		jsonl_syscall_open("fcntl", __NR_fcntl, true);
	jsonl_arg_open(0, "fd", side);
	jsonl_fd(fd, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "op", side);
	jsonl_const(F_GETFD, true, "F_GETFD", NULL);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("fcntl", __NR_fcntl, false);
	}
	printf("], \"return\": {");
	jsonl_flags_open_dflt_null(rc);
	jsonl_flags_const(FD_CLOEXEC, "FD_CLOEXEC", true);
	jsonl_flags_close();
	printf("}}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_fcntl && __NR_memfd_create")

#endif
