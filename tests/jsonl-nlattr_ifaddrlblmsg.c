/*
 * Check JSONL output of IFAL_* netlink attributes of ifaddrlblmsg.
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

# include <linux/if_addrlabel.h>

static void
init_ifaddrlblmsg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = RTM_GETADDRLABEL,
		.nlmsg_flags = NLM_F_DUMP
	);

	struct ifaddrlblmsg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct ifaddrlblmsg, msg,
		.ifal_family = AF_UNIX,
		.ifal_index = ifindex_lo()
	);
}

static void
print_ifaddrlblmsg(const unsigned int msg_len)
{
	jsonl_nlmsghdr_dump_open(msg_len, ARG_STR(RTM_GETADDRLABEL));
	jsonl_struct_open();
	jsonl_field_const("ifal_family", true, AF_UNIX, "AF_UNIX", NULL);
	jsonl_field_unsigned("ifal_prefixlen", false, 0);
	jsonl_field_unsigned("ifal_flags", false, 0);
	jsonl_field_ifindex("ifal_index", false, ifindex_lo(), "lo");
	jsonl_field_unsigned("ifal_seq", false, 0);
	jsonl_struct_close();
	printf("}");
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	const unsigned int hdrlen = sizeof(struct ifaddrlblmsg);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen), NLA_HDRLEN + 4);

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	const unsigned int nla_type = 0xffff & NLA_TYPE_MASK;
	JSONL_NLATTR_(fd, nlh0, hdrlen, init_ifaddrlblmsg, print_ifaddrlblmsg,
		      nla_type, NULL, "IFAL_???",
		      4, pattern, 4,
		      jsonl_bytes_value(pattern, 4, false));

	/* The address family of the message has no address decoder. */
	JSONL_NLATTR(fd, nlh0, hdrlen, init_ifaddrlblmsg, print_ifaddrlblmsg,
		     IFAL_ADDRESS, 4, pattern, 4,
		     jsonl_bytes_value(pattern, 4, false));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
