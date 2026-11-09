/*
 * Check JSONL output of prctl PR_MCE_KILL/PR_MCE_KILL_GET operations.
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

static void
emit_hex_arg(unsigned int index, const char *name, uint64_t val,
	     const char *side)
{
	jsonl_arg_open(index, name, side);
	printf("\"type\": \"hex\", \"raw\": ");
	jsonl_xlat_raw(val);
	jsonl_arg_close(side);
}

static void
emit_const_arg(unsigned int index, const char *name, uint64_t val,
	       const char *sym, const char *table, const char *side)
{
	jsonl_arg_open(index, name, side);
	jsonl_const(val, true, sym, table);
	jsonl_arg_close(side);
}

static void
emit_set(unsigned int policy, const char *sym, const char *table, long rc)
{
	const char *side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("prctl", __NR_prctl);
	else
		jsonl_syscall_open("prctl", __NR_prctl, true);
	emit_const_arg(0, "op", PR_MCE_KILL, "PR_MCE_KILL", NULL, side);
	printf(", ");
	emit_const_arg(1, "operation", PR_MCE_KILL_SET, "PR_MCE_KILL_SET", NULL,
		       side);
	printf(", ");
	emit_const_arg(2, "policy", policy, sym, table, side);
	printf(", ");
	emit_hex_arg(3, "arg4", 0, side);
	printf(", ");
	emit_hex_arg(4, "arg5", 0, side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("prctl", __NR_prctl, false);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
}

static void
emit_get(const uint64_t args[4], long rc)
{
	const char *side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("prctl", __NR_prctl);
	else
		jsonl_syscall_open("prctl", __NR_prctl, true);
	emit_const_arg(0, "op", PR_MCE_KILL_GET, "PR_MCE_KILL_GET", NULL, side);
	printf(", ");
	emit_hex_arg(1, "arg2", args[0], side);
	printf(", ");
	emit_hex_arg(2, "arg3", args[1], side);
	printf(", ");
	emit_hex_arg(3, "arg4", args[2], side);
	printf(", ");
	emit_hex_arg(4, "arg5", args[3], side);
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
		case 0: sym = "PR_MCE_KILL_LATE"; break;
		case 1: sym = "PR_MCE_KILL_EARLY"; break;
		case 2: sym = "PR_MCE_KILL_DEFAULT"; break;
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

	static const uint64_t get_args[4] = { 0, 0, 0, 0 };
	static const uint64_t get_bogus[4] = { 0xaaaa, 0xbbbb, 0xcccc, 0xdddd };

	emit_set(PR_MCE_KILL_EARLY, "PR_MCE_KILL_EARLY", NULL,
		 syscall(__NR_prctl, PR_MCE_KILL, PR_MCE_KILL_SET,
			 PR_MCE_KILL_EARLY, 0, 0));
	emit_get(get_args, syscall(__NR_prctl, PR_MCE_KILL_GET, 0, 0, 0, 0));

	emit_set(PR_MCE_KILL_DEFAULT, "PR_MCE_KILL_DEFAULT", NULL,
		 syscall(__NR_prctl, PR_MCE_KILL, PR_MCE_KILL_SET,
			 PR_MCE_KILL_DEFAULT, 0, 0));
	emit_get(get_args, syscall(__NR_prctl, PR_MCE_KILL_GET, 0, 0, 0, 0));

	emit_get(get_bogus, syscall(__NR_prctl, PR_MCE_KILL_GET,
				    0xaaaa, 0xbbbb, 0xcccc, 0xdddd));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("!__ia64__")

#endif
