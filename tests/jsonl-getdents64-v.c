/*
 * Check JSONL output of directory entries printed in full (-v): the dirp
 * argument of getdents64, an array of struct values, one per entry.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_getdents64

# include "jsonl.h"
# include "kernel_dirent.h"

# include <fcntl.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/stat.h>

static const char dname[] = "jsonl-getdents64-v.dir";

static const char *
dtype_sym(const unsigned char type)
{
	switch (type) {
	case 4: return "DT_DIR";
	case 8: return "DT_REG";
	default: return NULL;
	}
}

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const exit_side = merged ? "exit" : NULL;
	static char buf[4096];

	(void) rmdir(dname);
	if (mkdir(dname, 0700))
		perror_msg_and_fail("mkdir: %s", dname);
	const int fd = open(dname, O_RDONLY | O_DIRECTORY);
	if (fd < 0)
		perror_msg_and_fail("open: %s", dname);

	const long rc = syscall(__NR_getdents64, fd, buf, sizeof(buf));
	if (rc <= 0)
		perror_msg_and_fail("getdents64");

	if (merged)
		jsonl_syscall_merged_open("getdents64", __NR_getdents64);
	else
		jsonl_syscall_open("getdents64", __NR_getdents64, true);
	jsonl_arg_open(0, "fd", merged ? "entry" : NULL);
	jsonl_fd(fd, NULL, false);
	jsonl_arg_close(merged ? "entry" : NULL);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("getdents64", __NR_getdents64, false);
	} else {
		printf(", ");
	}
	jsonl_arg_open(1, "dirp", exit_side);
	jsonl_array_open();
	for (long off = 0; off < rc;) {
		const kernel_dirent64_t *const d =
			(const kernel_dirent64_t *) (buf + off);

		printf("%s{", off ? ", " : "");
		jsonl_struct_open();
		jsonl_field_unsigned("d_ino", true, d->d_ino);
		jsonl_field_unsigned("d_off", false, d->d_off);
		jsonl_field_unsigned("d_reclen", false, d->d_reclen);
		jsonl_field_open("d_type", false);
		jsonl_const(d->d_type, true, dtype_sym(d->d_type), "DT_???");
		jsonl_field_close();
		jsonl_field_open("d_name", false);
		printf("\"type\": \"path\", \"value\": ");
		jsonl_bytes(d->d_name);
		jsonl_field_close();
		jsonl_struct_close();
		printf("}");
		off += d->d_reclen;
	}
	jsonl_array_close();
	jsonl_arg_close(exit_side);
	printf(", ");
	jsonl_arg_open(2, "count", exit_side);
	jsonl_unsigned(sizeof(buf));
	jsonl_arg_close(exit_side);
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");

	close(fd);
	if (rmdir(dname))
		perror_msg_and_fail("rmdir: %s", dname);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_getdents64")

#endif
