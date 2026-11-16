/*
 * Check JSONL output of the getxattrat syscall whose args argument is
 * printed on exiting only: the entering event carries no element for it.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_getxattrat

# include "jsonl.h"

# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <linux/xattr.h>

int
main(int argc, char *argv[])
{
	static const char name[] = "user.x";
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const entry = merged ? "entry" : NULL;
	const char *const exit = merged ? "exit" : NULL;

	TAIL_ALLOC_OBJECT_CONST_PTR(struct xattr_args, args);
	char *const value = tail_alloc(1);
	memset(args, 0, sizeof(*args));
	args->value = (uintptr_t) value;
	args->size = 1;

	long rc = syscall(__NR_getxattrat, -1, "", 0, name, args,
			  sizeof(*args));
	int err = errno;

	if (merged)
		jsonl_syscall_merged_open("getxattrat", __NR_getxattrat);
	else
		jsonl_syscall_open("getxattrat", __NR_getxattrat, true);

	jsonl_arg_open(0, "dirfd", entry);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(entry);
	printf(", ");
	jsonl_arg_open(1, "pathname", entry);
	jsonl_path_value("");
	jsonl_arg_close(entry);
	printf(", ");
	jsonl_arg_open(2, "flags", entry);
	jsonl_flags_open(0);
	jsonl_flags_close();
	jsonl_arg_close(entry);
	printf(", ");
	jsonl_arg_open(3, "name", entry);
	jsonl_string_value(name, sizeof(name) - 1, false);
	jsonl_arg_close(entry);

	if (merged) {
		printf(", ");
	} else {
		printf("]}\n");
		jsonl_syscall_open("getxattrat", __NR_getxattrat, false);
	}

	jsonl_arg_open(4, "args", exit);
	jsonl_struct_open();
	jsonl_field_open("value", true);
	jsonl_addr(value);
	jsonl_field_close();
	jsonl_field_unsigned("size", false, 1);
	jsonl_field_open("flags", false);
	jsonl_flags_open(0);
	jsonl_flags_close();
	jsonl_field_close();
	jsonl_struct_close();
	jsonl_arg_close(exit);
	printf(", ");
	jsonl_arg_open(5, "size", exit);
	jsonl_unsigned(sizeof(*args));
	jsonl_arg_close(exit);

	printf("], ");
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_getxattrat")

#endif
