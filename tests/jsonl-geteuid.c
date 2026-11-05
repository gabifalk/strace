/*
 * Check JSONL output of the geteuid syscall entry and exit events.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_geteuid

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>

int
main(int argc, char *argv[])
{
	unsigned long rc = syscall(__NR_geteuid);

	if (argc > 1 && !strcmp(argv[1], "merged")) {
		jsonl_syscall_merged_open("geteuid", __NR_geteuid);
		printf("], \"return\": {\"type\": \"uid\", \"raw\": \"%lu\"}}\n",
		       rc);
		return 0;
	}

	jsonl_syscall_open("geteuid", __NR_geteuid, true);
	printf("]}\n");
	jsonl_syscall_open("geteuid", __NR_geteuid, false);
	printf("], \"return\": {\"type\": \"uid\", \"raw\": \"%lu\"}}\n", rc);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_geteuid")

#endif
