/*
 * Check JSONL output of the entry number written back by set_thread_area
 * as a set_thread_area_result value.  The syscall is asked to allocate
 * a free TLS entry, which fails on x86_64, where set_thread_area is not
 * implemented for 64-bit processes.  On i386, libc calls set_thread_area
 * before main, so the output of the test starts with a sched_yield call.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_set_thread_area && defined __NR_sched_yield \
 && (defined __x86_64__ || defined __i386__)

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <asm/ldt.h>

static bool merged;

static void
print_sched_yield(long rc)
{
	if (merged) {
		jsonl_syscall_merged_open("sched_yield", __NR_sched_yield);
	} else {
		jsonl_syscall_open("sched_yield", __NR_sched_yield, true);
		printf("]}\n");
		jsonl_syscall_open("sched_yield", __NR_sched_yield, false);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "entry" : NULL;

	TAIL_ALLOC_OBJECT_CONST_PTR(struct user_desc, desc);
	memset(desc, 0, sizeof(*desc));
	desc->entry_number = -1;
	desc->seg_32bit = 1;
	desc->contents = 1;

	print_sched_yield(syscall(__NR_sched_yield));

	const long rc = syscall(__NR_set_thread_area, desc);
	const int err = errno;

	if (merged)
		jsonl_syscall_merged_open("set_thread_area",
					  __NR_set_thread_area);
	else
		jsonl_syscall_open("set_thread_area", __NR_set_thread_area,
				   true);
	jsonl_arg_open(0, "u_info", side);
	jsonl_struct_open();
	jsonl_field_decimal("entry_number", true, -1);
	printf(", \"base_addr\": {\"type\": \"hex\", \"raw\": \"0\"},"
	       " \"limit\": {\"type\": \"hex\", \"raw\": \"0\"}");
	jsonl_field_unsigned("seg_32bit", false, 1);
	jsonl_field_unsigned("contents", false, 1);
	jsonl_field_unsigned("read_exec_only", false, 0);
	jsonl_field_unsigned("limit_in_pages", false, 0);
	jsonl_field_unsigned("seg_not_present", false, 0);
	jsonl_field_unsigned("useable", false, 0);
# ifdef __x86_64__
	jsonl_field_unsigned("lm", false, 0);
# endif
	jsonl_struct_close();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("set_thread_area", __NR_set_thread_area,
				   false);
	}
	printf("], ");
	if (rc) {
		errno = err;
		jsonl_return("unsigned", rc);
	} else {
		printf("\"return\": {\"type\": \"set_thread_area_result\","
		       " \"raw\": \"0\", \"entry_number\": {");
		jsonl_unsigned(desc->entry_number);
		printf("}}");
	}
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_set_thread_area && __NR_sched_yield"
		    " && (__x86_64__ || __i386__)")

#endif
