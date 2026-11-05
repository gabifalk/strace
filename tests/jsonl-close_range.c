/*
 * Check JSONL output of the close_range syscall's flags argument.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_close_range

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>

# ifndef CLOSE_RANGE_UNSHARE
#  define CLOSE_RANGE_UNSHARE (1U << 1)
# endif
# ifndef CLOSE_RANGE_CLOEXEC
#  define CLOSE_RANGE_CLOEXEC (1U << 2)
# endif

static bool merged;
static const char *side;

static void
emit(void (*flags)(void), long rc, int err)
{
	if (merged)
		jsonl_syscall_merged_open("close_range", __NR_close_range);
	else
		jsonl_syscall_open("close_range", __NR_close_range, true);
	jsonl_arg_open(0, "first", side);
	printf("\"type\": \"unsigned\", \"raw\": \"1\"");
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "last", side);
	printf("\"type\": \"unsigned\", \"raw\": \"0\"");
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "flags", side);
	flags();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("close_range", __NR_close_range, false);
	}
	printf("], ");
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");
}

static void
flags_known(void)
{
	jsonl_flags_open(CLOSE_RANGE_UNSHARE | CLOSE_RANGE_CLOEXEC);
	jsonl_flags_const(CLOSE_RANGE_UNSHARE, "CLOSE_RANGE_UNSHARE", true);
	jsonl_flags_const(CLOSE_RANGE_CLOEXEC, "CLOSE_RANGE_CLOEXEC", false);
	jsonl_flags_close();
}

static void
flags_mixed(void)
{
	jsonl_flags_open(CLOSE_RANGE_CLOEXEC | 0x10);
	jsonl_flags_const(CLOSE_RANGE_CLOEXEC, "CLOSE_RANGE_CLOEXEC", true);
	jsonl_flags_remainder(0x10, NULL, false);
	jsonl_flags_close();
}

static void
flags_unknown(void)
{
	jsonl_flags_open(0x10);
	jsonl_flags_remainder(0x10, "CLOSE_RANGE_???", true);
	jsonl_flags_close();
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	long rc1 = syscall(__NR_close_range, 1, 0,
			   CLOSE_RANGE_UNSHARE | CLOSE_RANGE_CLOEXEC);
	int err1 = errno;
	long rc2 = syscall(__NR_close_range, 1, 0, CLOSE_RANGE_CLOEXEC | 0x10);
	int err2 = errno;
	long rc3 = syscall(__NR_close_range, 1, 0, 0x10);
	int err3 = errno;

	emit(flags_known, rc1, err1);
	emit(flags_mixed, rc2, err2);
	emit(flags_unknown, rc3, err3);
	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_close_range")

#endif
