/*
 * Check JSONL output of NETNSA_* netlink attributes of rtgenmsg.
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

# include <linux/rtnetlink.h>

static void
init_rtgenmsg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = RTM_GETNSID,
		.nlmsg_flags = NLM_F_DUMP
	);

	struct rtgenmsg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct rtgenmsg, msg,
		.rtgen_family = AF_UNIX
	);
}

static void
print_rtgenmsg(const unsigned int msg_len)
{
	jsonl_nlmsghdr_dump_open(msg_len, ARG_STR(RTM_GETNSID));
	jsonl_struct_open();
	jsonl_field_const("rtgen_family", true, AF_UNIX, "AF_UNIX", NULL);
	jsonl_struct_close();
	printf("}");
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	const unsigned int hdrlen = sizeof(struct rtgenmsg);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen), NLA_HDRLEN + 4);

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	const unsigned int nla_type = 0xffff & NLA_TYPE_MASK;
	JSONL_NLATTR_(fd, nlh0, hdrlen, init_rtgenmsg, print_rtgenmsg,
		      nla_type, NULL, "NETNSA_???",
		      4, pattern, 4,
		      jsonl_bytes_value(pattern, 4, false));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
