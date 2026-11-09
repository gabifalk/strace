/*
 * Check JSONL output of the string arguments of the setxattr syscall.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_setxattr

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>

/* Default maximum number of string bytes strace prints. */
# define STRSIZE 32

static bool merged;
static const char *side;

static const char path[] = "/nonexistent_xyz";
static const char name[] = "user.x";

static void
emit(const char *value, size_t size, long rc, int err)
{
	const size_t vlen = size > STRSIZE ? STRSIZE : size;

	if (merged)
		jsonl_syscall_merged_open("setxattr", __NR_setxattr);
	else
		jsonl_syscall_open("setxattr", __NR_setxattr, true);

	jsonl_arg_open(0, "pathname", side);
	printf("\"type\": \"path\", \"value\": ");
	jsonl_bytes(path);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(1, "name", side);
	printf("\"type\": \"string\", \"value\": ");
	jsonl_bytes(name);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(2, "value", side);
	printf("\"type\": \"string\", \"value\": ");
	jsonl_bytes_n(value, vlen);
	if (size > STRSIZE)
		printf(", \"truncated\": true");
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(3, "size", side);
	printf("\"type\": \"unsigned\", \"raw\": \"%zu\"", size);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(4, "flags", side);
	jsonl_flags_open(0);
	jsonl_flags_close();
	jsonl_arg_close(side);

	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("setxattr", __NR_setxattr, false);
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

	static const char long_value[] =
		"\0\1\2\3cat test test test 0123456789abcdef";
	static const char short_value[] = "hello";

	const size_t long_size = sizeof(long_value) - 1;
	const size_t short_size = sizeof(short_value) - 1;

	long rc;
	int err;

	/* a value longer than the print limit is truncated */
	rc = syscall(__NR_setxattr, path, name, long_value, long_size, 0);
	err = errno;
	emit(long_value, long_size, rc, err);

	/* a short value is printed in full */
	rc = syscall(__NR_setxattr, path, name, short_value, short_size, 0);
	err = errno;
	emit(short_value, short_size, rc, err);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_setxattr")

#endif
