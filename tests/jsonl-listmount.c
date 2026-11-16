/*
 * Check JSONL output of a request structure of listmount that cannot be
 * read past its size field: a struct with the rest_unreadable meta-field.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_listmount

# include "jsonl.h"

# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const exit_side = merged ? "exit" : NULL;
	TAIL_ALLOC_OBJECT_CONST_PTR(uint32_t, size);

	*size = 24;
	const long rc = syscall(__NR_listmount, size, 0, 0, 0);
	const int err = errno;

	if (merged) {
		jsonl_syscall_merged_open("listmount", __NR_listmount);
		jsonl_arg_open(0, "req", "entry");
	} else {
		jsonl_syscall_open("listmount", __NR_listmount, true);
		jsonl_arg_open(0, "req", NULL);
	}
	jsonl_struct_open();
	jsonl_field_unsigned("size", true, *size);
	jsonl_struct_close();
	printf(", \"rest_unreadable\": true");
	jsonl_arg_close(merged ? "entry" : NULL);
	if (merged) {
		printf(", ");
	} else {
		printf("]}\n");
		jsonl_syscall_open("listmount", __NR_listmount, false);
	}
	jsonl_arg_open(1, "mnt_ids", exit_side);
	jsonl_addr(NULL);
	jsonl_arg_close(exit_side);
	printf(", ");
	jsonl_arg_open(2, "nr_mnt_ids", exit_side);
	jsonl_unsigned(0);
	jsonl_arg_close(exit_side);
	printf(", ");
	jsonl_arg_open(3, "flags", exit_side);
	jsonl_flags_open(0);
	jsonl_flags_close();
	jsonl_arg_close(exit_side);
	printf("], ");
	errno = err;
	jsonl_return("unsigned", rc);
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_listmount")

#endif
