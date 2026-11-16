/*
 * Check JSONL output of the tail of an extensible structure: the non-zero
 * bytes beyond the known part of struct file_attr (extra_data) and an
 * unreadable tail (rest_unreadable).
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_file_setattr

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>

# define KNOWN_SIZE 24
# define FULL_SIZE 32

static bool merged;
static const char *side;

static void
field_zero_flags(const char *name, bool first)
{
	jsonl_field_open(name, first);
	jsonl_flags_open(0);
	jsonl_flags_close();
	jsonl_field_close();
}

static void
test_file_setattr(const unsigned char *attr, bool tail_readable)
{
	long rc = syscall(__NR_file_setattr, -1, "", attr, FULL_SIZE, 0);
	int err = errno;

	if (merged)
		jsonl_syscall_merged_open("file_setattr", __NR_file_setattr);
	else
		jsonl_syscall_open("file_setattr", __NR_file_setattr, true);
	jsonl_arg_open(0, "dirfd", side);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "pathname", side);
	printf("\"type\": \"path\", \"value\": []");
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "attr", side);
	jsonl_struct_open();
	field_zero_flags("fa_xflags", true);
	jsonl_field_open("fa_extsize", false);
	jsonl_unsigned(0);
	jsonl_field_close();
	jsonl_field_open("fa_projid", false);
	jsonl_hex(0);
	jsonl_field_close();
	jsonl_field_open("fa_cowextsize", false);
	jsonl_unsigned(0);
	jsonl_field_close();
	jsonl_struct_close();
	if (tail_readable) {
		printf(", \"extra_data\": {\"type\": \"bytes\","
		       " \"offset\": \"%u\", \"size\": \"%u\", \"value\": ",
		       KNOWN_SIZE, FULL_SIZE - KNOWN_SIZE);
		jsonl_bytes_n((const char *) attr + KNOWN_SIZE,
			      FULL_SIZE - KNOWN_SIZE);
		printf("}");
	} else {
		printf(", \"rest_unreadable\": true");
	}
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(3, "size", side);
	jsonl_unsigned(FULL_SIZE);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(4, "flags", side);
	jsonl_flags_open(0);
	jsonl_flags_close();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("file_setattr", __NR_file_setattr, false);
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

	unsigned char *const full = tail_alloc(FULL_SIZE);
	unsigned char *const short_tail = tail_alloc(KNOWN_SIZE);

	memset(full, 0, KNOWN_SIZE);
	for (unsigned int i = KNOWN_SIZE; i < FULL_SIZE; ++i)
		full[i] = 0x80 + i - KNOWN_SIZE;
	memset(short_tail, 0, KNOWN_SIZE);

	test_file_setattr(full, true);
	test_file_setattr(short_tail, false);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_file_setattr")

#endif
