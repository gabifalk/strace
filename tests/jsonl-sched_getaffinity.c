/*
 * Check JSONL output of the CPU set of sched_getaffinity, a cpu_set_t value
 * whose elems are the numbers of the CPUs in the set.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"
#include "jsonl.h"

#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "entry" : NULL;
	const char *const exit_side = merged ? "exit" : NULL;
	TAIL_ALLOC_OBJECT_CONST_PTR(cpu_set_t, cpuset);

	const long rc = syscall(__NR_sched_getaffinity, 0, sizeof(*cpuset),
				cpuset);
	if (rc <= 0)
		perror_msg_and_skip("sched_getaffinity");

	if (merged)
		jsonl_syscall_merged_open("sched_getaffinity",
					  __NR_sched_getaffinity);
	else
		jsonl_syscall_open("sched_getaffinity",
				   __NR_sched_getaffinity, true);
	jsonl_arg_open(0, "pid", side);
	jsonl_pid("tgid", 0);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "cpusetsize", side);
	jsonl_unsigned(sizeof(*cpuset));
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("sched_getaffinity",
				   __NR_sched_getaffinity, false);
	} else {
		printf(", ");
	}
	jsonl_arg_open(2, "mask", exit_side);
	printf("\"type\": \"cpu_set_t\", \"elems\": [");
	for (unsigned int i = 0, n = 0; i < (unsigned long) rc * 8; ++i) {
		if (CPU_ISSET_S(i, (size_t) rc, cpuset))
			printf("%s\"%u\"", n++ ? ", " : "", i);
	}
	printf("]");
	jsonl_arg_close(exit_side);
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}
