/*
 * Check JSONL output of the data dumped by -e write=set: the io_dump
 * field of the write syscall event, a bytes value, and the size and the
 * fetch_failed_addr of data that cannot be fetched.
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

static bool merged;

static void
test_write(const int fd, const char *const buf, const size_t len,
	   const bool readable)
{
	const char *const side = merged ? "entry" : NULL;
	const long rc = syscall(__NR_write, fd, buf, len);
	const int err = errno;

	if (merged)
		jsonl_syscall_merged_open("write", __NR_write);
	else
		jsonl_syscall_open("write", __NR_write, true);
	jsonl_arg_open(0, "fd", side);
	jsonl_fd(fd, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "buf", side);
	if (readable) {
		printf("\"type\": \"string\", \"value\": ");
		jsonl_bytes_n(buf, len);
	} else {
		jsonl_addr(buf);
	}
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "count", side);
	jsonl_unsigned(len);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("write", __NR_write, false);
	}
	printf("], ");
	errno = err;
	jsonl_return("unsigned", rc);
	printf(", \"io_dump\": {\"type\": \"bytes\", \"value\": ");
	if (readable) {
		jsonl_bytes_n(buf, len);
	} else {
		printf("[], \"size\": \"%zu\", \"fetch_failed_addr\": \"%#lx\"",
		       len, (unsigned long) (uintptr_t) buf);
	}
	printf("}}\n");
}

int
main(int argc, char *argv[])
{
	static const char data[] = "0123456789abcdef\x00\x7f\x80\xff";
	char *const buf = tail_memdup(data, sizeof(data) - 1);

	merged = argc > 1 && !strcmp(argv[1], "merged");

	const int fd = open("/dev/null", O_WRONLY);
	if (fd < 0)
		perror_msg_and_fail("open: %s", "/dev/null");

	test_write(fd, buf, sizeof(data) - 1, true);
	test_write(fd, buf + sizeof(data) - 1, 3, false);

	jsonl_exit_event(0);
	return 0;
}
