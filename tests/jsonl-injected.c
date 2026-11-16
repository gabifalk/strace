/*
 * Check JSONL output of injected return values: the injected marker
 * of a return value, either a retval or an error.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_sched_yield && defined __NR_vhangup

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>

static bool merged;

static void
print_event(const char *name, long scno, long rc)
{
	if (merged) {
		jsonl_syscall_merged_open(name, scno);
	} else {
		jsonl_syscall_open(name, scno, true);
		printf("]}\n");
		jsonl_syscall_open(name, scno, false);
	}
	printf("], ");
	jsonl_return_injected("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	long rc = syscall(__NR_sched_yield);
	print_event("sched_yield", __NR_sched_yield, rc);

	rc = syscall(__NR_vhangup);
	print_event("vhangup", __NR_vhangup, rc);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sched_yield && __NR_vhangup")

#endif
