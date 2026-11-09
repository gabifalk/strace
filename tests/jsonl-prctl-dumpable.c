/*
 * Check JSONL output of prctl PR_SET_DUMPABLE/PR_GET_DUMPABLE operations.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"
#include "scno.h"
#include <linux/prctl.h>

#if !defined __ia64__

# include <stdio.h>
# include <string.h>
# include <unistd.h>

static bool merged;

static long
do_prctl(kernel_ulong_t op, kernel_ulong_t arg2)
{
	static const kernel_ulong_t bogus_arg =
		(kernel_ulong_t) 0xdeadbeefbadc0dedULL;
	return syscall(__NR_prctl, op, arg2, bogus_arg);
}

static void
prctl_event_open(unsigned int op, const char *op_sym)
{
	const char *side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("prctl", __NR_prctl);
	else
		jsonl_syscall_open("prctl", __NR_prctl, true);
	jsonl_arg_open(0, "op", side);
	jsonl_const(op, true, op_sym, NULL);
	jsonl_arg_close(side);
}

static void
emit_set(unsigned int dumpable, const char *sym, const char *table, long rc)
{
	const char *side = merged ? "entry" : NULL;

	prctl_event_open(PR_SET_DUMPABLE, "PR_SET_DUMPABLE");
	printf(", ");
	jsonl_arg_open(1, "dumpable", side);
	jsonl_const(dumpable, true, sym, table);
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
emit_get(long rc)
{
	prctl_event_open(PR_GET_DUMPABLE, "PR_GET_DUMPABLE");
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("prctl", __NR_prctl, false);
	}
	printf("], ");
	if (rc == -1) {
		jsonl_return("unsigned", rc);
	} else {
		const char *sym = NULL;

		switch (rc) {
		case 0: sym = "SUID_DUMP_DISABLE"; break;
		case 1: sym = "SUID_DUMP_USER"; break;
		case 2: sym = "SUID_DUMP_ROOT"; break;
		}
		printf("\"return\": {");
		jsonl_const((uint64_t) rc, false, sym, NULL);
		printf("}");
	}
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	emit_set(3, NULL, "SUID_DUMP_???", do_prctl(PR_SET_DUMPABLE, 3));

	emit_set(0, "SUID_DUMP_DISABLE", NULL, do_prctl(PR_SET_DUMPABLE, 0));
	emit_get(do_prctl(PR_GET_DUMPABLE, 0));

	emit_set(1, "SUID_DUMP_USER", NULL, do_prctl(PR_SET_DUMPABLE, 1));
	emit_get(do_prctl(PR_GET_DUMPABLE, 0));

	emit_set(2, "SUID_DUMP_ROOT", NULL, do_prctl(PR_SET_DUMPABLE, 2));
	emit_get(do_prctl(PR_GET_DUMPABLE, 0));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("!__ia64__")

#endif
