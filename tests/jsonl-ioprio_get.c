/*
 * Check JSONL output of the priority returned by ioprio_get as an ioprio
 * value.  The test is run with ioprio_get returning an injected value.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_ioprio_get

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "entry" : NULL;

	const long rc = syscall(__NR_ioprio_get, 1, 0);

	if (merged)
		jsonl_syscall_merged_open("ioprio_get", __NR_ioprio_get);
	else
		jsonl_syscall_open("ioprio_get", __NR_ioprio_get, true);
	jsonl_arg_open(0, "which", side);
	jsonl_const(1, true, "IOPRIO_WHO_PROCESS", NULL);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "who", side);
	jsonl_pid("tgid", 0);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("ioprio_get", __NR_ioprio_get, false);
	}
	printf("], \"return\": {\"type\": \"ioprio\", \"raw\": \"%#lx\","
	       " \"class\": {", rc);
	jsonl_const(rc >> 13, true, "IOPRIO_CLASS_BE", NULL);
	printf("}, \"level\": \"%lu\", \"injected\": true}}\n",
	       (unsigned long) rc & 0x1fff);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioprio_get")

#endif
