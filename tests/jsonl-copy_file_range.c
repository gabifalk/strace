/*
 * Check JSONL output of the copy_file_range syscall entry and exit events.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_copy_file_range

# include "jsonl.h"

# include <inttypes.h>

static const char *side;

static void
jsonl_off(unsigned int index, const char *name, int64_t val)
{
	jsonl_arg_open(index, name, side);
	printf("\"type\": \"decimal\", \"raw\": \"%" PRId64 "\", "
	       "\"indirect\": true", val);
	jsonl_arg_close(side);
}

int
main(int argc, char *argv[])
{
	TAIL_ALLOC_OBJECT_CONST_PTR(int64_t, off_in);
	TAIL_ALLOC_OBJECT_CONST_PTR(int64_t, off_out);
	*off_in = -2;
	*off_out = 1000000000000;

	const size_t len = 42;
	long rc = syscall(__NR_copy_file_range, -1, off_in, -2, off_out,
			  len, 0);
	int saved_errno = errno;

	side = (argc > 1 && !strcmp(argv[1], "merged")) ? "entry" : NULL;

	if (side)
		jsonl_syscall_merged_open("copy_file_range",
					  __NR_copy_file_range);
	else
		jsonl_syscall_open("copy_file_range", __NR_copy_file_range,
				   true);

	jsonl_arg_open(0, "fd_in", side);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_off(1, "off_in", *off_in);
	printf(", ");
	jsonl_arg_open(2, "fd_out", side);
	jsonl_fd(-2, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_off(3, "off_out", *off_out);
	printf(", ");
	jsonl_arg_open(4, "len", side);
	printf("\"type\": \"unsigned\", \"raw\": \"%zu\"", len);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(5, "flags", side);
	printf("\"type\": \"unsigned\", \"raw\": \"0\"");
	jsonl_arg_close(side);

	errno = saved_errno;
	if (side) {
		printf("], ");
		jsonl_return("decimal", rc);
		printf("}\n");
	} else {
		printf("]}\n");
		jsonl_syscall_open("copy_file_range", __NR_copy_file_range,
				   false);
		printf("], ");
		jsonl_return("decimal", rc);
		printf("}\n");
	}
	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_copy_file_range")

#endif
