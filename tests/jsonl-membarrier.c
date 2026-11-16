/*
 * Check JSONL output of the flags returned by membarrier as a flags
 * value.  The test is run with membarrier returning an injected value.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_membarrier

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <linux/membarrier.h>

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "entry" : NULL;

	const long rc = syscall(__NR_membarrier, MEMBARRIER_CMD_QUERY, 0, 0);

	if (merged)
		jsonl_syscall_merged_open("membarrier", __NR_membarrier);
	else
		jsonl_syscall_open("membarrier", __NR_membarrier, true);
	jsonl_arg_open(0, "cmd", side);
	jsonl_const(MEMBARRIER_CMD_QUERY, true, "MEMBARRIER_CMD_QUERY", NULL);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "flags", side);
	jsonl_flags_open(0);
	jsonl_flags_close();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("membarrier", __NR_membarrier, false);
	}
	printf("], \"return\": {");
	jsonl_flags_open_dflt_null(rc);
	jsonl_flags_const(MEMBARRIER_CMD_GLOBAL, "MEMBARRIER_CMD_GLOBAL", true);
	jsonl_flags_const(MEMBARRIER_CMD_GLOBAL_EXPEDITED,
			  "MEMBARRIER_CMD_GLOBAL_EXPEDITED", false);
	jsonl_flags_close();
	printf(", \"injected\": true}}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_membarrier")

#endif
