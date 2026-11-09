/*
 * Check JSONL output of the getresuid syscall entry and exit events.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_getresuid

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>

# if defined __NR_getresuid32 && __NR_getresuid != __NR_getresuid32
#  define UGID_TYPE	short
# else
#  define UGID_TYPE	int
# endif

static const char *side;

static void
jsonl_uid(unsigned int index, const char *name, unsigned val)
{
	jsonl_arg_open(index, name, side);
	printf("\"type\": \"uid\", \"raw\": \"%u\", \"indirect\": true",
	       val);
	jsonl_arg_close(side);
}

int
main(int argc, char *argv[])
{
	TAIL_ALLOC_OBJECT_CONST_PTR(unsigned UGID_TYPE, r);
	TAIL_ALLOC_OBJECT_CONST_PTR(unsigned UGID_TYPE, e);
	TAIL_ALLOC_OBJECT_CONST_PTR(unsigned UGID_TYPE, s);

	if (syscall(__NR_getresuid, r, e, s))
		perror_msg_and_skip("getresuid");

	if (argc > 1 && !strcmp(argv[1], "merged")) {
		side = "exit";
		jsonl_syscall_merged_open("getresuid", __NR_getresuid);
	} else {
		jsonl_syscall_open("getresuid", __NR_getresuid, true);
		printf("]}\n");
		jsonl_syscall_open("getresuid", __NR_getresuid, false);
	}
	jsonl_uid(0, "ruid", (unsigned) *r);
	printf(", ");
	jsonl_uid(1, "euid", (unsigned) *e);
	printf(", ");
	jsonl_uid(2, "suid", (unsigned) *s);
	printf("], \"return\": {\"type\": \"unsigned\", \"raw\": \"0\"}}\n");
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_getresuid")

#endif
