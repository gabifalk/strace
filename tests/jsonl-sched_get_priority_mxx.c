/*
 * Check JSONL output of the sched_get_priority_min and sched_get_priority_max
 * syscalls.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"
#include "scno.h"

#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void
jsonl_sched_get_priority(const char *name, long scno, const char *policy,
			 unsigned int policy_val, long rc, bool merged)
{
	const char *side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open(name, scno);
	else
		jsonl_syscall_open(name, scno, true);
	jsonl_arg_open(0, "policy", side);
	printf("\"type\": \"const\", \"raw\": \"%#x\", \"sym\": \"%s\"",
	       policy_val, policy);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open(name, scno, false);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");

	long min = syscall(__NR_sched_get_priority_min, SCHED_FIFO);
	long max = syscall(__NR_sched_get_priority_max, SCHED_RR);

	jsonl_sched_get_priority("sched_get_priority_min",
				 __NR_sched_get_priority_min,
				 "SCHED_FIFO", SCHED_FIFO, min, merged);
	jsonl_sched_get_priority("sched_get_priority_max",
				 __NR_sched_get_priority_max,
				 "SCHED_RR", SCHED_RR, max, merged);
	jsonl_exit_event(0);
	return 0;
}
