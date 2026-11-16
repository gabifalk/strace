/*
 * Check JSONL output of times, whose only argument is printed on exiting:
 * the entering event has no arguments and buf keeps index 0.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_times

# include "jsonl.h"

# include <errno.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "exit" : NULL;
	void *const buf = (void *) 0x42;

	long rc = syscall(__NR_times, buf);
	int err = errno;

	if (merged) {
		jsonl_syscall_merged_open("times", __NR_times);
	} else {
		jsonl_syscall_open("times", __NR_times, true);
		printf("]}\n");
		jsonl_syscall_open("times", __NR_times, false);
	}
	jsonl_arg_open(0, "buf", side);
	jsonl_addr(buf);
	jsonl_arg_close(side);
	printf("], ");
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_times")

#endif
