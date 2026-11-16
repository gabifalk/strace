/*
 * Check JSONL output of get_thread_area, whose u_info argument is a split
 * structure: entry_number is printed on entering, the rest on exiting.
 * The test runs where get_thread_area fails with ENOSYS, so the exiting
 * part is absent.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_get_thread_area && defined __x86_64__

# include "jsonl.h"

# include <errno.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <asm/ldt.h>

static bool merged;
static const char *side;

static void
get_thread_area_open(void)
{
	if (merged)
		jsonl_syscall_merged_open("get_thread_area",
					  __NR_get_thread_area);
	else
		jsonl_syscall_open("get_thread_area", __NR_get_thread_area,
				   true);
}

static void
get_thread_area_close(long rc, int err)
{
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("get_thread_area", __NR_get_thread_area,
				   false);
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

	TAIL_ALLOC_OBJECT_CONST_PTR(struct user_desc, desc);
	memset(desc, 0, sizeof(*desc));
	desc->entry_number = 12;

	long rc = syscall(__NR_get_thread_area, desc);
	int err = errno;
	if (rc != -1 || err != ENOSYS)
		error_msg_and_skip("get_thread_area does not fail with ENOSYS");

	get_thread_area_open();
	jsonl_arg_open(0, "u_info", side);
	jsonl_struct_open();
	jsonl_field_unsigned("entry_number", true, 12);
	jsonl_struct_close();
	printf(", \"split\": true");
	jsonl_arg_close(side);
	get_thread_area_close(rc, err);

	rc = syscall(__NR_get_thread_area, NULL);
	err = errno;
	get_thread_area_open();
	jsonl_arg_open(0, "u_info", side);
	jsonl_addr(NULL);
	jsonl_arg_close(side);
	get_thread_area_close(rc, err);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_get_thread_area && __x86_64__")

#endif
