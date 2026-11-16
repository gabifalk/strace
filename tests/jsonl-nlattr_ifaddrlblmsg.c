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
	static const struct strval32 nlmsg_flags[] = {
		{ ARG_STR(NLM_F_DUMP) }
	};

	printf("{");
	jsonl_nlmsghdr_vals(msg_len, ARG_STR(RTM_GETADDRLABEL), NULL,
			    NLM_F_DUMP, nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	jsonl_struct_open();
	jsonl_field_open("ifal_family", true);
	jsonl_const(AF_UNIX, true, "AF_UNIX", NULL);
	jsonl_field_close();
	jsonl_field_open("ifal_prefixlen", false);
	jsonl_unsigned(0);
	jsonl_field_close();
	jsonl_field_open("ifal_flags", false);
	jsonl_unsigned(0);
	jsonl_field_close();
	jsonl_field_open("ifal_index", false);
	jsonl_ifindex(ifindex_lo(), "lo");
	jsonl_field_close();
	jsonl_field_open("ifal_seq", false);
	jsonl_unsigned(0);
	jsonl_field_close();
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
