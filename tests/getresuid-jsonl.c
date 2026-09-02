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
# include <unistd.h>

# if defined __NR_getresuid32 && __NR_getresuid != __NR_getresuid32
#  define UGID_TYPE	short
# else
#  define UGID_TYPE	int
# endif

/* One getresuid uid argument as strace decodes it (PRINT_VAL_ID). */
static void
jsonl_uid(const char *name, unsigned val)
{
	printf("{\"arg\": \"%s\", \"type\": \"unsigned\", \"raw\": \"%u\"}",
	       name, val);
}

int
main(void)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(unsigned UGID_TYPE, r);
	TAIL_ALLOC_OBJECT_CONST_PTR(unsigned UGID_TYPE, e);
	TAIL_ALLOC_OBJECT_CONST_PTR(unsigned UGID_TYPE, s);

	if (syscall(__NR_getresuid, r, e, s))
		perror_msg_and_skip("getresuid");

	jsonl_syscall_open("getresuid", __NR_getresuid, true);
	printf("]}\n");
	jsonl_syscall_open("getresuid", __NR_getresuid, false);
	jsonl_uid("ruid", (unsigned) *r);
	printf(", ");
	jsonl_uid("euid", (unsigned) *e);
	printf(", ");
	jsonl_uid("suid", (unsigned) *s);
	printf("], \"return\": {\"type\": \"unsigned\", \"raw\": \"0\"}}\n");
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_getresuid")

#endif
