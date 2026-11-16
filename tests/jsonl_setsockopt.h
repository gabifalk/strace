/*
 * Helpers for emitting expected JSONL output in setsockopt tests.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef STRACE_TESTS_JSONL_SETSOCKOPT_H
# define STRACE_TESTS_JSONL_SETSOCKOPT_H

# include "tests.h"
# include "jsonl.h"
# include "scno.h"

# include <errno.h>
# include <stdio.h>
# include <unistd.h>

/* Set by the test when it is run for the jsonl-merged mode. */
static bool jsonl_setsockopt_merged;

/*
 * Call setsockopt and emit its expected event, the option value given
 * by print_val.
 */
static inline void
jsonl_setsockopt(int fd, int level, const char *level_str, int name,
		 const char *name_str, const void *val, int len,
		 void (*print_val)(const void *val))
{
	const char *side = jsonl_setsockopt_merged ? "entry" : NULL;
	long rc = syscall(__NR_setsockopt, fd, level, name, val, len);
	int saved_errno = errno;

	if (jsonl_setsockopt_merged)
		jsonl_syscall_merged_open("setsockopt", __NR_setsockopt);
	else
		jsonl_syscall_open("setsockopt", __NR_setsockopt, true);

	jsonl_arg_open(0, "sockfd", side);
	jsonl_fd(fd, NULL, false);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(1, "level", side);
	jsonl_const(level, true, level_str, NULL);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(2, "optname", side);
	jsonl_const(name, true, name_str, NULL);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(3, "optval", side);
	print_val(val);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(4, "optlen", side);
	jsonl_decimal(len);
	jsonl_arg_close(side);

	if (!jsonl_setsockopt_merged) {
		printf("]}\n");
		jsonl_syscall_open("setsockopt", __NR_setsockopt, false);
	}
	printf("], ");
	errno = saved_errno;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

#endif /* !STRACE_TESTS_JSONL_SETSOCKOPT_H */
