/*
 * Check JSONL output of the in-out offset argument of sendfile: the value
 * read on entering, marked inout, and the updated value read on exiting,
 * marked changed.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"
#include "jsonl.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char fname[] = "jsonl-sendfile.tmp";
static bool merged;

static void
test_sendfile(const int out_fd, const int in_fd, kernel_ulong_t *const offset,
	      const unsigned int count)
{
	const kernel_ulong_t saved_offset = *offset;
	const long rc = syscall(__NR_sendfile, out_fd, in_fd, offset, count);
	const int err = errno;

	if (merged)
		jsonl_syscall_merged_open("sendfile", __NR_sendfile);
	else
		jsonl_syscall_open("sendfile", __NR_sendfile, true);
	jsonl_arg_open(0, "out_fd", merged ? "entry" : NULL);
	jsonl_fd(out_fd, NULL, false);
	jsonl_arg_close(merged ? "entry" : NULL);
	printf(", ");
	jsonl_arg_open(1, "in_fd", merged ? "entry" : NULL);
	jsonl_fd(in_fd, NULL, false);
	jsonl_arg_close(merged ? "entry" : NULL);
	printf(", ");
	jsonl_arg_open(2, "offset", merged ? "entry" : NULL);
	jsonl_unsigned(saved_offset);
	jsonl_indirect();
	printf(", \"inout\": true");
	if (merged) {
		printf("}");
	} else {
		printf("}]}\n");
		jsonl_syscall_open("sendfile", __NR_sendfile, false);
	}
	if (rc > 0) {
		if (merged)
			printf(", \"exit\": {");
		else
			jsonl_arg_open(2, "offset", NULL);
		jsonl_unsigned(*offset);
		jsonl_indirect();
		printf(", \"changed\": true}");
		if (!merged)
			printf(", ");
	}
	if (merged)
		printf("}, ");
	jsonl_arg_open(3, "count", merged ? "exit" : NULL);
	jsonl_unsigned(count);
	jsonl_arg_close(merged ? "exit" : NULL);
	printf("], ");
	errno = err;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	static const char data[] = "0123456789";
	TAIL_ALLOC_OBJECT_CONST_PTR(kernel_ulong_t, offset);

	merged = argc > 1 && !strcmp(argv[1], "merged");

	(void) unlink(fname);
	const int in_fd = open(fname, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (in_fd < 0)
		perror_msg_and_fail("open: %s", fname);
	if (write(in_fd, data, sizeof(data) - 1) != sizeof(data) - 1)
		perror_msg_and_fail("write");
	const int out_fd = open("/dev/null", O_WRONLY);
	if (out_fd < 0)
		perror_msg_and_fail("open: %s", "/dev/null");

	*offset = 2;
	test_sendfile(out_fd, in_fd, offset, 5);

	*offset = 3;
	test_sendfile(out_fd, -1, offset, 5);

	close(in_fd);
	if (unlink(fname))
		perror_msg_and_fail("unlink: %s", fname);

	jsonl_exit_event(0);
	return 0;
}
