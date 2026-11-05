/*
 * Check JSONL output of prctl PR_GET_TSC/PR_SET_TSC operations.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"
#include "scno.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/prctl.h>

static bool merged;

static void
jsonl_prctl_open(unsigned int op, const char *sym)
{
	const char *side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("prctl", __NR_prctl);
	else
		jsonl_syscall_open("prctl", __NR_prctl, true);
	jsonl_arg_open(0, "op", side);
	printf("\"type\": \"const\", \"raw\": \"%#x\", \"sym\": \"%s\"",
	       op, sym);
	jsonl_arg_close(side);
}

static void
jsonl_tsc_flag(unsigned int flag, const char *extra)
{
	printf("\"type\": \"const\", \"raw\": \"");
	if (flag)
		printf("%#x", flag);
	else
		printf("0");
	switch (flag) {
	case PR_TSC_ENABLE:
		printf("\", \"sym\": \"PR_TSC_ENABLE\"");
		break;
	case PR_TSC_SIGSEGV:
		printf("\", \"sym\": \"PR_TSC_SIGSEGV\"");
		break;
	default:
		printf("\", \"table\": \"PR_TSC_???\"");
	}
	printf("%s", extra);
}

static void
jsonl_set_tsc(unsigned int flag, long rc)
{
	const char *side = merged ? "entry" : NULL;

	jsonl_prctl_open(PR_SET_TSC, "PR_SET_TSC");
	printf(", ");
	jsonl_arg_open(1, "flag", side);
	jsonl_tsc_flag(flag, "");
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("prctl", __NR_prctl, false);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
}

static void
jsonl_get_tsc(const int *tsc, long rc)
{
	const char *side = merged ? "exit" : NULL;

	jsonl_prctl_open(PR_GET_TSC, "PR_GET_TSC");
	if (merged) {
		printf(", ");
	} else {
		printf("]}\n");
		jsonl_syscall_open("prctl", __NR_prctl, false);
	}
	jsonl_arg_open(1, "flag", side);
	if (rc == 0)
		jsonl_tsc_flag(*tsc, ", \"indirect\": true");
	else if (tsc)
		printf("\"type\": \"addr\", \"raw\": \"%#lx\"",
		       (unsigned long) (uintptr_t) tsc);
	else
		printf("\"type\": \"addr\", \"raw\": null");
	jsonl_arg_close(side);
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	static const kernel_ulong_t bogus_tsc =
		(kernel_ulong_t) 0xdeadc0defacebeefULL;

	TAIL_ALLOC_OBJECT_CONST_PTR(int, tsc);
	long rc;

	rc = syscall(__NR_prctl, PR_SET_TSC, 0);
	jsonl_set_tsc(0, rc);

	rc = syscall(__NR_prctl, PR_SET_TSC, bogus_tsc);
	jsonl_set_tsc((unsigned int) bogus_tsc, rc);

	rc = syscall(__NR_prctl, PR_SET_TSC, PR_TSC_SIGSEGV);
	jsonl_set_tsc(PR_TSC_SIGSEGV, rc);

	rc = syscall(__NR_prctl, PR_GET_TSC, NULL);
	jsonl_get_tsc(NULL, rc);

	rc = syscall(__NR_prctl, PR_GET_TSC, tsc + 1);
	jsonl_get_tsc(tsc + 1, rc);

	rc = syscall(__NR_prctl, PR_GET_TSC, tsc);
	jsonl_get_tsc(tsc, rc);

	jsonl_exit_event(0);
	return 0;
}
