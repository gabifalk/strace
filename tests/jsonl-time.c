/*
 * Check JSONL output of the time returned by time as a time_t value.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_time

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");

	const long rc = syscall(__NR_time, NULL);

	if (merged) {
		jsonl_syscall_merged_open("time", __NR_time);
	} else {
		jsonl_syscall_open("time", __NR_time, true);
		printf("]}\n");
		jsonl_syscall_open("time", __NR_time, false);
	}
	jsonl_arg_open(0, "tloc", merged ? "exit" : NULL);
	jsonl_addr(NULL);
	jsonl_arg_close(merged ? "exit" : NULL);
	printf("], \"return\": {\"type\": \"time_t\", \"raw\": \"%lu\"}}\n",
	       rc);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_time")

#endif
