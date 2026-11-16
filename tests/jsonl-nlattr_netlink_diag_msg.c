/*
 * Check JSONL output of NETLINK_DIAG_* netlink attributes of netlink_diag_msg.
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

# include <linux/netlink_diag.h>
# include <linux/sock_diag.h>

static void
init_netlink_diag_msg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = SOCK_DIAG_BY_FAMILY,
		.nlmsg_flags = NLM_F_DUMP
	);

	struct netlink_diag_msg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct netlink_diag_msg, msg,
		.ndiag_family = AF_NETLINK,
		.ndiag_type = SOCK_RAW,
		.ndiag_protocol = NETLINK_ROUTE,
		.ndiag_state = NETLINK_CONNECTED
	);
}

static void
print_netlink_diag_msg(const unsigned int msg_len)
{
	jsonl_nlmsghdr_dump_open(msg_len, ARG_STR(SOCK_DIAG_BY_FAMILY));
	jsonl_struct_open();
	jsonl_field_const("ndiag_family", true, AF_NETLINK, "AF_NETLINK", NULL);
	jsonl_field_const("ndiag_type", false, SOCK_RAW, "SOCK_RAW", NULL);
	jsonl_field_const("ndiag_protocol", false, NETLINK_ROUTE,
			  "NETLINK_ROUTE", NULL);
	jsonl_field_const("ndiag_state", false, NETLINK_CONNECTED,
			  "NETLINK_CONNECTED", NULL);
	jsonl_field_unsigned("ndiag_portid", false, 0);
	jsonl_field_unsigned("ndiag_dst_portid", false, 0);
	jsonl_field_unsigned("ndiag_dst_group", false, 0);
	jsonl_field_unsigned("ndiag_ino", false, 0);
	jsonl_field_open("ndiag_cookie", false);
	jsonl_array_open();
	printf("{");
	jsonl_unsigned(0);
	printf("}, {");
	jsonl_unsigned(0);
	printf("}");
	jsonl_array_close();
	jsonl_field_close();
	jsonl_struct_close();
	printf("}");
}

static void
print_xlong(const unsigned long *p, size_t i)
{
	jsonl_hex(*p);
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	static const unsigned long groups[] = {
		(unsigned long) 0xdeadbeefbadc0dedULL,
		(unsigned long) 0xdeadbeefbadc0dedULL
	};
	static const struct netlink_diag_ring ndr = {
		.ndr_block_size = 0xfabfabdc,
		.ndr_block_nr = 0xabcdabda,
		.ndr_frame_size = 0xcbadbafa,
		.ndr_frame_nr = 0xdbcafadb
	};
	static const uint32_t flags =
		NDIAG_FLAG_CB_RUNNING | NDIAG_FLAG_PKTINFO;
	static const struct strval32 ndiag_flags[] = {
		{ ARG_STR(NDIAG_FLAG_CB_RUNNING) },
		{ ARG_STR(NDIAG_FLAG_PKTINFO) },
	};

	const int fd = create_nl_socket(NETLINK_SOCK_DIAG);
	const unsigned int hdrlen = sizeof(struct netlink_diag_msg);
	void *const nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
					 NLA_HDRLEN +
					 MAX(sizeof(groups), sizeof(ndr)));

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	JSONL_NLATTR_ARRAY(fd, nlh0, hdrlen,
			   init_netlink_diag_msg, print_netlink_diag_msg,
			   NETLINK_DIAG_GROUPS, pattern, groups, print_xlong);

	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen,
			    init_netlink_diag_msg, print_netlink_diag_msg,
			    NETLINK_DIAG_RX_RING, pattern, ndr,
			    jsonl_struct_open();
			    jsonl_field_unsigned("ndr_block_size", true,
						 ndr.ndr_block_size);
			    jsonl_field_unsigned("ndr_block_nr", false,
						 ndr.ndr_block_nr);
			    jsonl_field_unsigned("ndr_frame_size", false,
						 ndr.ndr_frame_size);
			    jsonl_field_unsigned("ndr_frame_nr", false,
						 ndr.ndr_frame_nr);
			    jsonl_struct_close());

	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen,
			    init_netlink_diag_msg, print_netlink_diag_msg,
			    NETLINK_DIAG_FLAGS, pattern, flags,
			    jsonl_flags(flags, ndiag_flags,
					ARRAY_SIZE(ndiag_flags),
					"NDIAG_FLAG_???"));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
