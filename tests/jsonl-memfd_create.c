/*
 * Check JSONL output of a composed flags value: the flags argument of
 * memfd_create, whose flag bits and hugetlb page size shift are a single
 * group of elements.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_memfd_create

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>

# define F_CLOEXEC	0x1U
# define F_HUGETLB	0x4U
# define F_UNKNOWN	0x100U
# define HUGE_SHIFT	26
# define HUGE_2MB	(21U << HUGE_SHIFT)

static bool merged;
static const char *side;

static const char name[] = "jsonl_memfd";

static void
flags_composed(unsigned int flags)
{
	jsonl_flags_open(flags);
	jsonl_flags_const(F_CLOEXEC, "MFD_CLOEXEC", true);
	if (flags & F_HUGETLB)
		jsonl_flags_const(F_HUGETLB, "MFD_HUGETLB", false);
	if (flags & F_UNKNOWN)
		jsonl_flags_remainder(F_UNKNOWN, NULL, false);
	if (flags & HUGE_2MB)
		printf(", {\"type\": \"shift\", \"raw\": \"%#x\","
		       " \"value\": \"21\", \"shift\": \"MFD_HUGE_SHIFT\"}",
		       HUGE_2MB);
	jsonl_flags_close();
}

static void
test_memfd_create(unsigned int flags)
{
	long rc = syscall(__NR_memfd_create, name, flags);
	int err = errno;

	if (merged)
		jsonl_syscall_merged_open("memfd_create", __NR_memfd_create);
	else
		jsonl_syscall_open("memfd_create", __NR_memfd_create, true);
	jsonl_arg_open(0, "name", side);
	jsonl_string_value(name, sizeof(name) - 1, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "flags", side);
	if (flags)
		flags_composed(flags);
	else
		jsonl_hex(0);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("memfd_create", __NR_memfd_create, false);
	}
	printf("], ");
	errno = err;
	jsonl_return("fd", rc);
	printf("}\n");

	if (rc >= 0)
		close(rc);
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	test_memfd_create(F_CLOEXEC | F_HUGETLB | HUGE_2MB);
	test_memfd_create(F_CLOEXEC | F_UNKNOWN);
	test_memfd_create(0);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_memfd_create")

#endif
