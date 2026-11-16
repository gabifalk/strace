/*
 * Check JSONL output of NETLINK_SELINUX messages.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_sendto

# include "jsonl_netlink.h"

# include <linux/selinux_netlink.h>

static const struct strval32 nlmsg_flags[] = {
	{ ARG_STR(NLM_F_REQUEST) }
};

static void
test_nlmsg_type(const int fd)
{
	const struct nlmsghdr nlh = {
		.nlmsg_len = sizeof(nlh),
		.nlmsg_type = SELNL_MSG_SETENFORCE,
		.nlmsg_flags = NLM_F_REQUEST,
	};

	jsonl_nl_sendto(fd, &nlh, sizeof(nlh));
	jsonl_nl_sendto_open(fd);
	jsonl_nlmsghdr(&nlh, "SELNL_MSG_SETENFORCE", NULL,
		       nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	jsonl_nl_sendto_close();
}

static void
test_selnl_msg_unspec(const int fd)
{
	static const char data[] = "1234";
	void *const nlh0 = midtail_alloc(NLMSG_HDRLEN, sizeof(data) - 1);

	JSONL_NETLINK_(fd, nlh0,
		       0xffff, NULL, "SELNL_MSG_???",
		       NLM_F_REQUEST, nlmsg_flags, ARRAY_SIZE(nlmsg_flags),
		       sizeof(data) - 1, data, sizeof(data) - 1,
		       jsonl_bytes_value(data, sizeof(data) - 1, false));
}

static void
test_selnl_msg_setenforce(const int fd)
{
	static const struct selnl_msg_setenforce msg = {
		.val = 0xfbdcdfab
	};
	void *const nlh0 = midtail_alloc(NLMSG_HDRLEN, sizeof(msg));

	JSONL_NETLINK_OBJECT(fd, nlh0,
			     SELNL_MSG_SETENFORCE, NLM_F_REQUEST, msg,
			     jsonl_struct_open();
			     jsonl_field_decimal("val", true, msg.val);
			     jsonl_struct_close());
}

static void
test_selnl_msg_policyload(const int fd)
{
	static const struct selnl_msg_policyload msg = {
		.seqno = 0xabdcfabc
	};
	void *const nlh0 = midtail_alloc(NLMSG_HDRLEN, sizeof(msg));

	JSONL_NETLINK_OBJECT(fd, nlh0,
			     SELNL_MSG_POLICYLOAD, NLM_F_REQUEST, msg,
			     jsonl_struct_open();
			     jsonl_field_unsigned("seqno", true, msg.seqno);
			     jsonl_struct_close());
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	int fd = create_nl_socket(NETLINK_SELINUX);

	test_nlmsg_type(fd);
	test_selnl_msg_unspec(fd);
	test_selnl_msg_setenforce(fd);
	test_selnl_msg_policyload(fd);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
