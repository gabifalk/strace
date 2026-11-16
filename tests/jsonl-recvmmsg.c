/*
 * Check JSONL output of recvmmsg: the msgvec delivered as split
 * struct mmsghdr values, the timeout saved on entering and printed on
 * exiting, and recvmmsg_result return values with the time left, or
 * a timeout.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_recvmmsg

# include "jsonl.h"
# include "kernel_old_timespec.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/socket.h>
# include <sys/uio.h>

static bool merged;

/*
 * The msgvec entry half is the array of split struct mmsghdr values
 * holding the split msg_hdr with the msg_namelen fetched on entering.
 */
static void
open_event(int fd, const struct mmsghdr *mh, unsigned int vlen)
{
	if (merged)
		jsonl_syscall_merged_open("recvmmsg", __NR_recvmmsg);
	else
		jsonl_syscall_open("recvmmsg", __NR_recvmmsg, true);
	jsonl_arg_open(0, "sockfd", merged ? "entry" : NULL);
	jsonl_fd(fd, NULL, false);
	jsonl_arg_close(merged ? "entry" : NULL);
	if (vlen) {
		printf(", ");
		jsonl_arg_open(1, "msgvec", merged ? "entry" : NULL);
		jsonl_array_open();
		for (unsigned int i = 0; i < vlen; ++i) {
			printf("%s{", i ? ", " : "");
			jsonl_struct_open();
			printf("\"msg_hdr\": {");
			jsonl_struct_open();
			jsonl_field_decimal("msg_namelen", true,
					    mh[i].msg_hdr.msg_namelen);
			jsonl_struct_close();
			printf(", \"split\": true}");
			jsonl_struct_close();
			printf(", \"split\": true}");
		}
		jsonl_array_close();
		if (merged)
			printf("}, \"exit\": {");
		else
			jsonl_arg_close(NULL);
	}
	if (merged) {
		if (!vlen) {
			printf(", ");
			jsonl_arg_open(1, "msgvec", "exit");
		}
	} else {
		printf("]}\n");
		jsonl_syscall_open("recvmmsg", __NR_recvmmsg, false);
		jsonl_arg_open(1, "msgvec", NULL);
	}
	jsonl_array_open();
}

static void
close_args(unsigned int vlen, const kernel_old_timespec_t *ts)
{
	const char *const side = merged ? "exit" : NULL;

	jsonl_array_close();
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "vlen", side);
	jsonl_unsigned(vlen);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(3, "flags", side);
	jsonl_flags_open(0);
	jsonl_flags_close();
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(4, "timeout", side);
	jsonl_timespec(ts->tv_sec, ts->tv_nsec);
	jsonl_arg_close(side);
	printf("], \"return\": {\"type\": \"recvmmsg_result\"");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	int sv[2];
	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sv))
		perror_msg_and_skip("socketpair");

	struct mmsghdr mh[1];
	memset(mh, 0, sizeof(mh));
	const kernel_old_timespec_t ts0 = { .tv_sec = 1, .tv_nsec = 2 };
	kernel_old_timespec_t ts = ts0;
	long rc = syscall(__NR_recvmmsg, sv[0], mh, 0, 0, &ts);
	open_event(sv[0], mh, 0);
	close_args(0, &ts0);
	printf(", \"raw\": \"%ld\"}}\n", rc);

	char c = 'x', buf;
	if (write(sv[1], &c, 1) != 1)
		perror_msg_and_fail("write");
	struct iovec iov = { .iov_base = &buf, .iov_len = 1 };
	mh[0].msg_hdr.msg_iov = &iov;
	mh[0].msg_hdr.msg_iovlen = 1;
	const kernel_old_timespec_t ts1 = { .tv_sec = 1 };
	ts = ts1;
	rc = syscall(__NR_recvmmsg, sv[0], mh, 1, 0, &ts);
	open_event(sv[0], mh, 1);
	printf("{");
	jsonl_struct_open();
	printf("\"msg_hdr\": {");
	jsonl_struct_open();
	printf("\"msg_name\": {");
	jsonl_addr(NULL);
	printf("}");
	jsonl_field_decimal("msg_namelen", false, 0);
	printf(", \"msg_iov\": {");
	jsonl_array_open();
	printf("{");
	jsonl_struct_open();
	printf("\"iov_base\": {\"type\": \"string\", \"value\": ");
	jsonl_bytes_n(&buf, 1);
	printf("}");
	jsonl_field_unsigned("iov_len", false, 1);
	jsonl_struct_close();
	printf("}");
	jsonl_array_close();
	printf("}");
	jsonl_field_unsigned("msg_iovlen", false, 1);
	jsonl_field_unsigned("msg_controllen", false, 0);
	printf(", \"msg_flags\": {");
	jsonl_flags_open(0);
	jsonl_flags_close();
	printf("}");
	jsonl_struct_close();
	printf(", \"split\": true}");
	jsonl_field_unsigned("msg_len", false, mh[0].msg_len);
	jsonl_struct_close();
	printf(", \"split\": true}");
	close_args(1, &ts1);
	printf(", \"raw\": \"%ld\", \"left\": {", rc);
	jsonl_timespec(ts.tv_sec, ts.tv_nsec);
	printf("}}}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_recvmmsg")

#endif
