/*
 * Check JSONL output of the prctl PR_SET_PDEATHSIG/PR_GET_PDEATHSIG signal.
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

# include <signal.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

static bool merged;

static void
op_open(unsigned int op, const char *op_sym)
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

/* PR_SET_PDEATHSIG with an out-of-range value printed as a bare number. */
static void
emit_set_raw(uint64_t sig, long rc)
{
	const char *side = merged ? "entry" : NULL;

	op_open(PR_SET_PDEATHSIG, "PR_SET_PDEATHSIG");
	printf(", ");
	jsonl_arg_open(1, "sig", side);
	printf("\"type\": \"unsigned\", \"raw\": \"%" PRIu64 "\"", sig);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("prctl", __NR_prctl, false);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
}

/* PR_SET_PDEATHSIG with a valid signal number. */
static void
emit_set_sig(int sig, const char *sym, long rc)
{
	const char *side = merged ? "entry" : NULL;

	op_open(PR_SET_PDEATHSIG, "PR_SET_PDEATHSIG");
	printf(", ");
	jsonl_arg_open(1, "sig", side);
	jsonl_signal(sig, sym);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("prctl", __NR_prctl, false);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
}

/* PR_GET_PDEATHSIG writes the signal to its pointer, decoded on exiting. */
static void
emit_get(int sig, const char *sym, long rc)
{
	op_open(PR_GET_PDEATHSIG, "PR_GET_PDEATHSIG");
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("prctl", __NR_prctl, false);
	} else {
		printf(", ");
	}
	jsonl_arg_open(1, "sig", merged ? "exit" : NULL);
	jsonl_signal(sig, sym);
	printf(", \"indirect\": true");
	jsonl_arg_close(merged ? "exit" : NULL);
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	static const kernel_ulong_t bogus =
		(kernel_ulong_t) 0xbadc0deddeadfeedULL;

	merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_PTR(int, pdeathsig);

	emit_set_raw((uint64_t) bogus,
		     syscall(__NR_prctl, PR_SET_PDEATHSIG, bogus));
	emit_set_sig(SIGINT, "SIGINT",
		     syscall(__NR_prctl, PR_SET_PDEATHSIG, (long) SIGINT));
	emit_get(SIGINT, "SIGINT",
		 syscall(__NR_prctl, PR_GET_PDEATHSIG, pdeathsig));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("!__ia64__")

#endif
