/*
 * Check JSONL output of the mseal syscall.
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

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const unsigned long addr = (unsigned long) 0xfacefeeddeadbeefULL;
	const unsigned long len = (unsigned long) 0xcafef00dbadc0dedULL;
	const unsigned long flags = -1UL;
	long rc = syscall(__NR_mseal, addr, len, flags);

	const char *side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("mseal", __NR_mseal);
	else
		jsonl_syscall_open("mseal", __NR_mseal, true);
	jsonl_arg_open(0, "addr", side);
	printf("\"type\": \"addr\", \"raw\": \"%#lx\"", addr);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "length", side);
	printf("\"type\": \"unsigned\", \"raw\": \"%lu\"", len);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "flags", side);
	printf("\"type\": \"hex\", \"raw\": \"%#lx\"", flags);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("mseal", __NR_mseal, false);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
	jsonl_exit_event(0);
	return 0;
}
