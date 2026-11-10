/*
 * Check JSONL output of the prctl PR_SCHED_CORE_GET operation, whose
 * cookie argument dereferences to a hex-formatted integer.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_prctl

# include "jsonl.h"

# include <inttypes.h>
# include <linux/prctl.h>

# ifndef PR_SCHED_CORE
#  define PR_SCHED_CORE 62
# endif
# ifndef PR_SCHED_CORE_GET
#  define PR_SCHED_CORE_GET 0
# endif
# ifndef PIDTYPE_PID
#  define PIDTYPE_PID 0
# endif

int
main(int argc, char *argv[])
{
	TAIL_ALLOC_OBJECT_CONST_PTR(uint64_t, cookie);
	const int tid = syscall(__NR_gettid);

	long rc = syscall(__NR_prctl, PR_SCHED_CORE, PR_SCHED_CORE_GET,
			  (long) tid, PIDTYPE_PID, cookie);
	if (rc)
		perror_msg_and_skip("prctl PR_SCHED_CORE_GET");

	const int merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *eside = merged ? "entry" : NULL;
	const char *xside = merged ? "exit" : NULL;

	if (merged)
		jsonl_syscall_merged_open("prctl", __NR_prctl);
	else
		jsonl_syscall_open("prctl", __NR_prctl, true);

	jsonl_arg_open(0, "op", eside);
	jsonl_const(PR_SCHED_CORE, true, "PR_SCHED_CORE", NULL);
	jsonl_arg_close(eside);
	printf(", ");
	jsonl_arg_open(1, "operation", eside);
	jsonl_const(PR_SCHED_CORE_GET, false, "PR_SCHED_CORE_GET", NULL);
	jsonl_arg_close(eside);
	printf(", ");
	jsonl_arg_open(2, "pid", eside);
	printf("\"type\": \"decimal\", \"raw\": \"%d\"", tid);
	jsonl_arg_close(eside);
	printf(", ");
	jsonl_arg_open(3, "pid_type", eside);
	jsonl_const(PIDTYPE_PID, false, "PIDTYPE_PID", NULL);
	jsonl_arg_close(eside);

	if (merged) {
		printf(", ");
	} else {
		printf("]}\n");
		jsonl_syscall_open("prctl", __NR_prctl, false);
	}

	jsonl_arg_open(4, "cookie", xside);
	printf("\"type\": \"hex\", \"raw\": \"%#" PRIx64 "\", \"indirect\": true",
	       *cookie);
	jsonl_arg_close(xside);

	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_prctl")

#endif
