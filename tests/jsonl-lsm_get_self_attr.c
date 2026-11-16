/*
 * Check JSONL output of the lsm_get_self_attr syscall without
 * LSM_FLAG_SINGLE: the ctx argument is printed on exiting only, and the
 * entering event carries no element for it.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_lsm_get_self_attr

# include "jsonl.h"

# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <linux/lsm.h>

int
main(int argc, char *argv[])
{
	static const unsigned int bad_flags = 2;
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const entry = merged ? "entry" : NULL;
	const char *const exit = merged ? "exit" : NULL;

	TAIL_ALLOC_OBJECT_CONST_PTR(uint32_t, size);
	void *const ctx = tail_alloc(1);
	*size = 1;

	long rc = syscall(__NR_lsm_get_self_attr, LSM_ATTR_CURRENT, ctx, size,
			  bad_flags);
	int err = errno;

	if (merged)
		jsonl_syscall_merged_open("lsm_get_self_attr",
					  __NR_lsm_get_self_attr);
	else
		jsonl_syscall_open("lsm_get_self_attr",
				   __NR_lsm_get_self_attr, true);

	jsonl_arg_open(0, "attr", entry);
	jsonl_const(LSM_ATTR_CURRENT, true, "LSM_ATTR_CURRENT", NULL);
	jsonl_arg_close(entry);
	printf(", ");

	if (merged) {
		jsonl_arg_open(1, "ctx", exit);
		jsonl_addr(ctx);
		jsonl_arg_close(exit);
		printf(", ");
		jsonl_arg_open(2, "size", entry);
		jsonl_unsigned(*size);
		jsonl_indirect();
		printf(", \"inout\": true}, \"exit\": {");
		jsonl_unsigned(*size);
		jsonl_indirect();
		jsonl_arg_close(exit);
	} else {
		jsonl_arg_open(2, "size", NULL);
		jsonl_unsigned(*size);
		jsonl_indirect();
		printf(", \"inout\": true");
		jsonl_arg_close(NULL);
		printf("]}\n");
		jsonl_syscall_open("lsm_get_self_attr",
				   __NR_lsm_get_self_attr, false);
		jsonl_arg_open(1, "ctx", NULL);
		jsonl_addr(ctx);
		jsonl_arg_close(NULL);
		printf(", ");
		jsonl_arg_open(2, "size", NULL);
		jsonl_unsigned(*size);
		jsonl_indirect();
		jsonl_arg_close(NULL);
	}
	printf(", ");
	jsonl_arg_open(3, "flags", exit);
	jsonl_flags_open(bad_flags);
	jsonl_flags_remainder(bad_flags, "LSM_FLAG_???", true);
	jsonl_flags_close();
	jsonl_arg_close(exit);

	printf("], ");
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_lsm_get_self_attr")

#endif
