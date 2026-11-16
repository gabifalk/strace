/*
 * Check JSONL output of the getgid syscall entry and exit events.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_getgid

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>

int
main(int argc, char *argv[])
{
	unsigned long rc = syscall(__NR_getgid);

	if (argc > 1 && !strcmp(argv[1], "merged")) {
		jsonl_syscall_merged_open("getgid", __NR_getgid);
		printf("], \"return\": {\"type\": \"gid\", \"raw\": \"%lu\"}}\n",
		       rc);
		jsonl_exit_event(0);
		return 0;
	}

	jsonl_syscall_open("getgid", __NR_getgid, true);
	printf("]}\n");
	jsonl_syscall_open("getgid", __NR_getgid, false);
	printf("], \"return\": {\"type\": \"gid\", \"raw\": \"%lu\"}}\n", rc);
	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_getgid")

#endif
