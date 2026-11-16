/*
 * Check JSONL output of the policy returned by sched_getscheduler as
 * a const value.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_sched_getscheduler

# include "jsonl.h"

# include <sched.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "entry" : NULL;

	const long rc = syscall(__NR_sched_getscheduler, 0);
	if (rc != SCHED_OTHER)
		error_msg_and_skip("sched_getscheduler: unexpected policy %ld",
				   rc);

	if (merged)
		jsonl_syscall_merged_open("sched_getscheduler", __NR_sched_getscheduler);
	else
		jsonl_syscall_open("sched_getscheduler", __NR_sched_getscheduler, true);
	jsonl_arg_open(0, "pid", side);
	jsonl_pid("tgid", 0);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("sched_getscheduler", __NR_sched_getscheduler, false);
	}
	printf("], \"return\": {");
	jsonl_const(SCHED_OTHER, true, "SCHED_OTHER", NULL);
	printf("}}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sched_getscheduler")

#endif
