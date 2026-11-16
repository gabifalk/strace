/*
 * Check JSONL output of a flags value of several groups: the swapflags
 * argument of swapon, whose priority is a group of its own.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_swapon

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>

# define F_PREFER		0x8000U
# define F_DISCARD		0x10000U
# define F_DISCARD_PAGES	0x40000U
# define PRIO_MASK		0x7fffU

static bool merged;
static const char *side;

static const char path[] = "/nonexistent/jsonl_swapon";

static void
test_swapon(unsigned int swapflags)
{
	const unsigned int flags = swapflags & ~PRIO_MASK;
	const unsigned int prio = swapflags & PRIO_MASK;
	long rc = syscall(__NR_swapon, path, swapflags);
	int err = errno;

	if (merged)
		jsonl_syscall_merged_open("swapon", __NR_swapon);
	else
		jsonl_syscall_open("swapon", __NR_swapon, true);
	jsonl_arg_open(0, "path", side);
	jsonl_path_value(path);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "swapflags", side);
	jsonl_flags_groups_open(swapflags);
	if (flags) {
		bool first = true;

		jsonl_flags_group_open(flags, true);
		if (flags & F_PREFER) {
			jsonl_flags_const(F_PREFER, "SWAP_FLAG_PREFER", first);
			first = false;
		}
		if (flags & F_DISCARD) {
			jsonl_flags_const(F_DISCARD, "SWAP_FLAG_DISCARD",
					  first);
			first = false;
		}
		if (flags & F_DISCARD_PAGES)
			jsonl_flags_const(F_DISCARD_PAGES,
					  "SWAP_FLAG_DISCARD_PAGES", first);
		jsonl_flags_group_close();
	}
	jsonl_flags_group_open(prio, !flags);
	printf("{");
	jsonl_unsigned(prio);
	printf("}");
	jsonl_flags_group_close();
	jsonl_flags_groups_close();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("swapon", __NR_swapon, false);
	}
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

	test_swapon(F_PREFER | 42);
	test_swapon(F_DISCARD | F_DISCARD_PAGES);
	test_swapon(0);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_swapon")

#endif
