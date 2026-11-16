/*
 * Check JSONL output of the data dumped by -e write=set for a vector of
 * buffers: the io_dump field of the writev syscall event, an array of
 * bytes values, one for each dumped buffer.
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
#include <sys/uio.h>

int
main(int argc, char *argv[])
{
	static const char data0[] = "ab";
	static const char data1[] = "cde";
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "entry" : NULL;
	struct iovec *const iov = tail_alloc(2 * sizeof(*iov));

	iov[0].iov_base = tail_memdup(data0, sizeof(data0) - 1);
	iov[0].iov_len = sizeof(data0) - 1;
	iov[1].iov_base = tail_memdup(data1, sizeof(data1) - 1);
	iov[1].iov_len = sizeof(data1) - 1;

	const int fd = open("/dev/null", O_WRONLY);
	if (fd < 0)
		perror_msg_and_fail("open: %s", "/dev/null");

	const long rc = syscall(__NR_writev, fd, iov, 2);

	if (merged)
		jsonl_syscall_merged_open("writev", __NR_writev);
	else
		jsonl_syscall_open("writev", __NR_writev, true);
	jsonl_arg_open(0, "fd", side);
	jsonl_fd(fd, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "iov", side);
	jsonl_array_open();
	for (unsigned int i = 0; i < 2; ++i) {
		printf("%s{", i ? ", " : "");
		jsonl_struct_open();
		jsonl_field_open("iov_base", true);
		printf("\"type\": \"string\", \"value\": ");
		jsonl_bytes_n(iov[i].iov_base, iov[i].iov_len);
		jsonl_field_close();
		jsonl_field_unsigned("iov_len", false, iov[i].iov_len);
		jsonl_struct_close();
		printf("}");
	}
	jsonl_array_close();
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "iovcnt", side);
	jsonl_unsigned(2);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("writev", __NR_writev, false);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf(", \"io_dump\": {\"type\": \"array\", \"elems\": [");
	for (unsigned int i = 0; i < 2; ++i) {
		printf("%s{", i ? ", " : "");
		jsonl_bytes_value(iov[i].iov_base, iov[i].iov_len, false);
		printf("}");
	}
	printf("]}}\n");

	jsonl_exit_event(0);
	return 0;
}
