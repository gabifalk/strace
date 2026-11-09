/*
 * Check JSONL output of prctl operations that take no arguments.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"
#include "scno.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/prctl.h>

static bool merged;
static const char *side;

static void
jsonl_hex_arg(unsigned int index, const char *name, kernel_ulong_t val)
{
	printf(", ");
	jsonl_arg_open(index, name, side);
	if (val)
		printf("\"type\": \"hex\", \"raw\": \"%#llx\"",
		       (unsigned long long) val);
	else
		printf("\"type\": \"hex\", \"raw\": \"0\"");
	jsonl_arg_close(side);
}

static void
jsonl_prctl(unsigned int op, const char *sym, bool with_args,
	    kernel_ulong_t arg2, kernel_ulong_t arg3,
	    kernel_ulong_t arg4, kernel_ulong_t arg5, long rc)
{
	if (merged)
		jsonl_syscall_merged_open("prctl", __NR_prctl);
	else
		jsonl_syscall_open("prctl", __NR_prctl, true);

	jsonl_arg_open(0, "op", side);
	printf("\"type\": \"const\", \"raw\": \"");
	if (op)
		printf("%#x", op);
	else
		printf("0");
	if (sym)
		printf("\", \"sym\": \"%s\"", sym);
	else
		printf("\", \"table\": \"PR_???\"");
	jsonl_arg_close(side);

	if (with_args) {
		jsonl_hex_arg(1, "arg2", arg2);
		jsonl_hex_arg(2, "arg3", arg3);
		jsonl_hex_arg(3, "arg4", arg4);
		jsonl_hex_arg(4, "arg5", arg5);
	}

	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("prctl", __NR_prctl, false);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	static const kernel_ulong_t bogus_op_bits =
		(kernel_ulong_t) 0xbadc0ded00000000ULL;
	static const kernel_ulong_t bogus_arg =
		(kernel_ulong_t) 0xfacefeeddeadbeefULL;
	static const struct strval32 options[] = {
		{  7, "PR_GET_KEEPCAPS" },
		{ 13, "PR_GET_TIMING" },
		{ 21, "PR_GET_SECCOMP" },
		{ 30, "PR_GET_TIMERSLACK" },
		{ 31, "PR_TASK_PERF_EVENTS_DISABLE" },
		{ 32, "PR_TASK_PERF_EVENTS_ENABLE" },
	};
	static const struct strval32 options_checking[] = {
		{ 39, "PR_GET_NO_NEW_PRIVS" },
		{ 42, "PR_GET_THP_DISABLE" },
		{ 58, "PR_GET_IO_FLUSHER" },
		{ 67, "PR_SET_MEMORY_MERGE" },
		{ 68, "PR_GET_MEMORY_MERGE" },
	};
	static const unsigned int options_unknown[] = {
		0, 17, 18, 48, 49, 82, 83, 84,
		0x41555855, /* "AUXV" - 1 */
		0x41555857, /* "AUXV" + 1 */
		0x53564d40, /* "SVMA" - 1 */
		0x53564d42, /* "SVMA" + 1 */
		0x59616d60, /* "Yama" - 1 */
		0x59616d62, /* "Yama" + 1 */
		0xffffffff,
	};
	long rc;

	for (const struct strval32 *i = options;
	     i < options + ARRAY_SIZE(options); i++) {
		rc = syscall(__NR_prctl, i->val | bogus_op_bits);
		jsonl_prctl(i->val, i->str, false, 0, 0, 0, 0, rc);
		rc = syscall(__NR_prctl, i->val, bogus_arg);
		jsonl_prctl(i->val, i->str, false, 0, 0, 0, 0, rc);
		rc = syscall(__NR_prctl, i->val, 0, bogus_arg);
		jsonl_prctl(i->val, i->str, false, 0, 0, 0, 0, rc);
		rc = syscall(__NR_prctl, i->val, 0, 0, bogus_arg);
		jsonl_prctl(i->val, i->str, false, 0, 0, 0, 0, rc);
		rc = syscall(__NR_prctl, i->val, 0, 0, 0, bogus_arg);
		jsonl_prctl(i->val, i->str, false, 0, 0, 0, 0, rc);
		rc = syscall(__NR_prctl, i->val, bogus_arg, bogus_arg + 1,
			     bogus_arg + 2, bogus_arg + 3);
		jsonl_prctl(i->val, i->str, false, 0, 0, 0, 0, rc);
	}

	for (const struct strval32 *i = options_checking;
	     i < options_checking + ARRAY_SIZE(options_checking); i++) {
		rc = syscall(__NR_prctl, i->val | bogus_op_bits, 0, 0, 0, 0);
		jsonl_prctl(i->val, i->str, true, 0, 0, 0, 0, rc);
		rc = syscall(__NR_prctl, i->val, bogus_arg, 0, 0, 0);
		jsonl_prctl(i->val, i->str, true, bogus_arg, 0, 0, 0, rc);
		rc = syscall(__NR_prctl, i->val, 0, bogus_arg, 0, 0);
		jsonl_prctl(i->val, i->str, true, 0, bogus_arg, 0, 0, rc);
		rc = syscall(__NR_prctl, i->val, 0, 0, bogus_arg, 0);
		jsonl_prctl(i->val, i->str, true, 0, 0, bogus_arg, 0, rc);
		rc = syscall(__NR_prctl, i->val, 0, 0, 0, bogus_arg);
		jsonl_prctl(i->val, i->str, true, 0, 0, 0, bogus_arg, rc);
		rc = syscall(__NR_prctl, i->val, bogus_arg, bogus_arg + 1,
			     bogus_arg + 2, bogus_arg + 3);
		jsonl_prctl(i->val, i->str, true, bogus_arg, bogus_arg + 1,
			    bogus_arg + 2, bogus_arg + 3, rc);
	}

	for (const unsigned int *i = options_unknown;
	     i < options_unknown + ARRAY_SIZE(options_unknown); i++) {
		rc = syscall(__NR_prctl, *i | bogus_op_bits, 0, 0, 0, 0);
		jsonl_prctl(*i, NULL, true, 0, 0, 0, 0, rc);
		rc = syscall(__NR_prctl, *i, bogus_arg, 0, 0, 0);
		jsonl_prctl(*i, NULL, true, bogus_arg, 0, 0, 0, rc);
		rc = syscall(__NR_prctl, *i, 0, bogus_arg, 0, 0);
		jsonl_prctl(*i, NULL, true, 0, bogus_arg, 0, 0, rc);
		rc = syscall(__NR_prctl, *i, 0, 0, bogus_arg, 0);
		jsonl_prctl(*i, NULL, true, 0, 0, bogus_arg, 0, rc);
		rc = syscall(__NR_prctl, *i, 0, 0, 0, bogus_arg);
		jsonl_prctl(*i, NULL, true, 0, 0, 0, bogus_arg, rc);
		rc = syscall(__NR_prctl, *i, bogus_arg, bogus_arg + 1,
			     bogus_arg + 2, bogus_arg + 3);
		jsonl_prctl(*i, NULL, true, bogus_arg, bogus_arg + 1,
			    bogus_arg + 2, bogus_arg + 3, rc);
	}

	jsonl_exit_event(0);
	return 0;
}
