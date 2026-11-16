/*
 * Check JSONL output of the hex dumps of sendmmsg: an io_dump array
 * with one array of dumped buffers for each message.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_sendmmsg

# include "jsonl.h"

# include <errno.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/socket.h>
# include <sys/uio.h>

static void
print_buffer(const struct iovec *iov, bool first)
{
	printf("%s{\"type\": \"bytes\", \"value\": ", first ? "" : ", ");
	jsonl_bytes_n(iov->iov_base, iov->iov_len);
	printf("}");
}

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	int sv[2];

	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sv))
		perror_msg_and_skip("socketpair");

	static char buf0[] = "012", buf1[] = "34", buf2[] = "56789";
	struct iovec iov[] = {
		{ .iov_base = buf0, .iov_len = sizeof(buf0) - 1 },
		{ .iov_base = buf1, .iov_len = sizeof(buf1) - 1 },
		{ .iov_base = buf2, .iov_len = sizeof(buf2) - 1 },
	};
	TAIL_ALLOC_OBJECT_CONST_ARR(struct mmsghdr, mh, 2);
	memset(mh, 0, sizeof(*mh) * 2);
	mh[0].msg_hdr.msg_iov = iov;
	mh[0].msg_hdr.msg_iovlen = 2;
	mh[1].msg_hdr.msg_iov = iov + 2;
	mh[1].msg_hdr.msg_iovlen = 1;

	long rc = syscall(__NR_sendmmsg, sv[0], mh, 2, 0);
	if (rc != 2)
		perror_msg_and_skip("sendmmsg");

	const char *const side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("sendmmsg", __NR_sendmmsg);
	else
		jsonl_syscall_open("sendmmsg", __NR_sendmmsg, true);
	jsonl_arg_open(0, "sockfd", side);
	jsonl_fd(sv[0], NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "msgvec", side);
	jsonl_addr(mh);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "vlen", side);
	jsonl_unsigned(2);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(3, "flags", side);
	jsonl_flags_open(0);
	jsonl_flags_close();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("sendmmsg", __NR_sendmmsg, false);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf(", \"io_dump\": {");
	jsonl_array_open();
	for (unsigned int i = 0; i < 2; ++i) {
		printf("%s{", i ? ", " : "");
		jsonl_array_open();
		for (unsigned int j = 0;
		     j < (unsigned int) mh[i].msg_hdr.msg_iovlen; ++j)
			print_buffer(&mh[i].msg_hdr.msg_iov[j], !j);
		jsonl_array_close();
		printf(", \"iovlen\": \"%u\"}",
		       (unsigned int) mh[i].msg_hdr.msg_iovlen);
	}
	jsonl_array_close();
	printf("}}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendmmsg")

#endif
