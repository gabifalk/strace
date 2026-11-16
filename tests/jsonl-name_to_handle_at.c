/*
 * Check JSONL output of the struct file_handle of name_to_handle_at:
 * the entering and exiting halves of the split struct, the change
 * of handle_bytes, and the f_handle bytes.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_name_to_handle_at

# include "jsonl.h"

# include <fcntl.h>
# include <stdio.h>
# include <string.h>

# ifndef MAX_HANDLE_SZ
#  define MAX_HANDLE_SZ 128
# endif

struct handle {
	unsigned int handle_bytes;
	int handle_type;
	unsigned char f_handle[MAX_HANDLE_SZ];
};

static bool merged;

static void
print_call(const struct handle *h, unsigned int entering_bytes,
	   const int *mount_id, long rc, int err)
{
	const char *const side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("name_to_handle_at",
					  __NR_name_to_handle_at);
	else
		jsonl_syscall_open("name_to_handle_at",
				   __NR_name_to_handle_at, true);
	jsonl_arg_open(0, "dirfd", side);
	jsonl_const_decimal(AT_FDCWD, "AT_FDCWD", NULL);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "pathname", side);
	jsonl_path_value(".");
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "handle", side);
	jsonl_struct_open();
	jsonl_field_unsigned("handle_bytes", true, entering_bytes);
	jsonl_struct_close();
	printf(", \"split\": true");
	if (merged) {
		printf("}, \"exit\": {");
	} else {
		printf("}]}\n");
		jsonl_syscall_open("name_to_handle_at",
				   __NR_name_to_handle_at, false);
		jsonl_arg_open(2, "handle", NULL);
	}
	jsonl_struct_open();
	jsonl_field_open("handle_bytes", true);
	if (h->handle_bytes != entering_bytes)
		printf("\"changed\": true, ");
	jsonl_unsigned(h->handle_bytes);
	jsonl_field_close();
	if (rc == 0) {
		jsonl_field_decimal("handle_type", false, h->handle_type);
		jsonl_field_open("f_handle", false);
		jsonl_bytes_value(h->f_handle, h->handle_bytes, false);
		jsonl_field_close();
	}
	jsonl_struct_close();
	printf(", \"split\": true");
	jsonl_arg_close(merged ? "exit" : NULL);
	printf(", ");
	jsonl_arg_open(3, "mount_id", merged ? "exit" : NULL);
	if (rc == 0) {
		jsonl_decimal(*mount_id);
		jsonl_indirect();
	} else {
		jsonl_addr(mount_id);
	}
	jsonl_arg_close(merged ? "exit" : NULL);
	printf(", ");
	jsonl_arg_open(4, "flags", merged ? "exit" : NULL);
	jsonl_flags(0, NULL, 0, NULL);
	jsonl_arg_close(merged ? "exit" : NULL);
	printf("], ");
	errno = err;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_PTR(struct handle, h);
	TAIL_ALLOC_OBJECT_CONST_PTR(int, mount_id);

	h->handle_bytes = 0;
	long rc = syscall(__NR_name_to_handle_at, AT_FDCWD, ".", h,
			  mount_id, 0);
	if (rc != -1 || errno != EOVERFLOW)
		perror_msg_and_skip("name_to_handle_at");
	print_call(h, 0, mount_id, rc, errno);

	const unsigned int bytes = h->handle_bytes;
	if (bytes > MAX_HANDLE_SZ)
		error_msg_and_skip("handle_bytes %u is too large", bytes);
	rc = syscall(__NR_name_to_handle_at, AT_FDCWD, ".", h, mount_id, 0);
	if (rc)
		perror_msg_and_fail("name_to_handle_at");
	print_call(h, bytes, mount_id, rc, 0);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_name_to_handle_at")

#endif
