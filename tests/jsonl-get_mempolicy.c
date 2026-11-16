/*
 * Check JSONL output of the get_mempolicy syscall.
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

#include "xlat.h"
#include "xlat/mpol_modes.h"

static bool merged;

static void
arg_open(unsigned int index, const char *name)
{
	if (index)
		printf(", ");
	jsonl_arg_open(index, name, merged ? "exit" : NULL);
}

static void
arg_close(void)
{
	jsonl_arg_close(merged ? "exit" : NULL);
}

static void
arg_addr(unsigned int index, const char *name, unsigned long addr)
{
	arg_open(index, name);
	jsonl_addr((const void *) addr);
	arg_close();
}

/* The mode is printed as a pointer unless mode_readable is set. */
static long
test_get_mempolicy(const int *mode, bool mode_readable,
		   const unsigned long *nodemask, unsigned long maxnode,
		   unsigned long addr, unsigned long flags)
{
	static const struct strval32 known[] = {
		{ 1, "MPOL_F_NODE" },
		{ 2, "MPOL_F_ADDR" },
		{ 4, "MPOL_F_MEMS_ALLOWED" }
	};

	long rc = syscall(__NR_get_mempolicy, mode, nodemask, maxnode,
			  addr, flags);
	int saved_errno = errno;

	if (merged) {
		jsonl_syscall_merged_open("get_mempolicy", __NR_get_mempolicy);
	} else {
		jsonl_syscall_open("get_mempolicy", __NR_get_mempolicy, true);
		printf("]}\n");
		jsonl_syscall_open("get_mempolicy", __NR_get_mempolicy, false);
	}

	if (mode_readable && rc == 0) {
		arg_open(0, "mode");
		jsonl_const((unsigned int) *mode, true,
			    jsonl_xlat_sym(mpol_modes, *mode), "MPOL_???");
		jsonl_indirect();
		arg_close();
	} else {
		arg_addr(0, "mode", (unsigned long) mode);
	}

	arg_addr(1, "nodemask", (unsigned long) nodemask);

	arg_open(2, "maxnode");
	jsonl_unsigned(maxnode);
	arg_close();

	arg_addr(3, "addr", addr);

	arg_open(4, "flags");
	jsonl_flags(flags, known, ARRAY_SIZE(known), NULL);
	arg_close();

	printf("], ");
	errno = saved_errno;
	jsonl_return("unsigned", rc);
	printf("}\n");

	return rc;
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	if (test_get_mempolicy(NULL, false, NULL, 0, 0, 0))
		perror_msg_and_skip("get_mempolicy");

	int *mode = (void *) 0xdefaced1baddeed2;
	unsigned long maxnode = (unsigned long) 0xcafef00dbadc0dedULL;
	const unsigned long *nodemask = (void *) 0xfacedad3bebefed4ULL;
	const unsigned long addr = (unsigned long) 0xfacefeeddeadbeefULL;
	const unsigned long flags = -1U;
	test_get_mempolicy(mode, false, nodemask, maxnode, addr, flags);

	mode = tail_alloc(sizeof(*mode));
	test_get_mempolicy(mode, true, NULL, 0, 0, 0);

	*mode = -1;
	test_get_mempolicy(mode, true, NULL, 0, (unsigned long) (mode - 1), 2);

	/* The nodemask is not writable. */
	nodemask = (const unsigned long *) (mode + 1);
	maxnode = sizeof(*nodemask) * 8;
	if (!test_get_mempolicy(NULL, false, nodemask, maxnode, 0, 0))
		error_msg_and_fail("get_mempolicy: unexpected success");

	jsonl_exit_event(0);
	return 0;
}
