/*
 * Check JSONL output of resource limits: the new_limit argument
 * of prlimit64, whose infinite limit is a named constant.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_prlimit64

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/resource.h>

# define TEST_PID	0x7fffffff
# define INFINITY64	(~0ULL)

static bool merged;
static const char *side;

static void
field_limit(const char *name, bool first, uint64_t val)
{
	jsonl_field_open(name, first);
	if (val == INFINITY64)
		jsonl_const(val, false, "RLIM64_INFINITY", NULL);
	else
		jsonl_unsigned(val);
	jsonl_field_close();
}

static void
test_prlimit64(uint64_t *lim)
{
	const char *const exit_side = merged ? "exit" : NULL;
	long rc = syscall(__NR_prlimit64, TEST_PID, RLIMIT_CORE, lim, NULL);
	int err = errno;

	if (merged)
		jsonl_syscall_merged_open("prlimit64", __NR_prlimit64);
	else
		jsonl_syscall_open("prlimit64", __NR_prlimit64, true);
	jsonl_arg_open(0, "pid", side);
	jsonl_pid("tgid", TEST_PID);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "resource", side);
	jsonl_const(RLIMIT_CORE, true, "RLIMIT_CORE", NULL);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "new_limit", side);
	jsonl_struct_open();
	field_limit("rlim_cur", true, lim[0]);
	field_limit("rlim_max", false, lim[1]);
	jsonl_struct_close();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("prlimit64", __NR_prlimit64, false);
	} else {
		printf(", ");
	}
	jsonl_arg_open(3, "old_limit", exit_side);
	jsonl_addr(NULL);
	jsonl_arg_close(exit_side);
	printf("], ");
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	TAIL_ALLOC_OBJECT_CONST_ARR(uint64_t, lim, 2);

	lim[0] = INFINITY64;
	lim[1] = 1024;
	test_prlimit64(lim);

	lim[0] = 0;
	lim[1] = INFINITY64 - 1;
	test_prlimit64(lim);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_prlimit64")

#endif
