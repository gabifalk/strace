/*
 * Check JSONL output of the mlockall syscall's flags argument.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_mlockall

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <sys/mman.h>
# include <unistd.h>

static bool merged;
static const char *side;

static void
flags_elems(void)
{
	jsonl_flags_open(MCL_CURRENT | MCL_FUTURE | 0x10000000);
	jsonl_flags_const(MCL_CURRENT, "MCL_CURRENT", true);
	jsonl_flags_const(MCL_FUTURE, "MCL_FUTURE", false);
	jsonl_flags_remainder(0x10000000, NULL, false);
	jsonl_flags_close();
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	long rc = syscall(__NR_mlockall, MCL_CURRENT | MCL_FUTURE | 0x10000000);
	int err = errno;

	if (merged)
		jsonl_syscall_merged_open("mlockall", __NR_mlockall);
	else
		jsonl_syscall_open("mlockall", __NR_mlockall, true);
	jsonl_arg_open(0, "flags", side);
	flags_elems();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("mlockall", __NR_mlockall, false);
	}
	printf("], ");
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");
	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_mlockall")

#endif
