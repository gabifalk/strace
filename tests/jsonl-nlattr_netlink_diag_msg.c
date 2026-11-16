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
print_u_field(const char *name, uint64_t val, bool first)
{
	jsonl_field_open(name, first);
	jsonl_unsigned(val);
	jsonl_field_close();
}

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
	static const struct strval32 nlmsg_flags[] = {
		{ ARG_STR(NLM_F_DUMP) }
	};

	printf("{");
	jsonl_nlmsghdr_vals(msg_len, ARG_STR(SOCK_DIAG_BY_FAMILY), NULL,
			    NLM_F_DUMP, nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	jsonl_struct_open();
	jsonl_field_open("ndiag_family", true);
	jsonl_const(AF_NETLINK, true, "AF_NETLINK", NULL);
	jsonl_field_close();
	jsonl_field_open("ndiag_type", false);
	jsonl_const(SOCK_RAW, true, "SOCK_RAW", NULL);
	jsonl_field_close();
	jsonl_field_open("ndiag_protocol", false);
	jsonl_const(NETLINK_ROUTE, true, "NETLINK_ROUTE", NULL);
	jsonl_field_close();
	jsonl_field_open("ndiag_state", false);
	jsonl_const(NETLINK_CONNECTED, true, "NETLINK_CONNECTED", NULL);
	jsonl_field_close();
	print_u_field("ndiag_portid", 0, false);
	print_u_field("ndiag_dst_portid", 0, false);
	print_u_field("ndiag_dst_group", 0, false);
	print_u_field("ndiag_ino", 0, false);
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
			    print_u_field("ndr_block_size", ndr.ndr_block_size,
					  true);
			    print_u_field("ndr_block_nr", ndr.ndr_block_nr,
					  false);
			    print_u_field("ndr_frame_size", ndr.ndr_frame_size,
					  false);
			    print_u_field("ndr_frame_nr", ndr.ndr_frame_nr,
					  false);
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
