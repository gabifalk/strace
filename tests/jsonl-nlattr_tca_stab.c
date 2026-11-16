/*
 * Check JSONL output of TCA_STAB_* netlink attributes of tcmsg.
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

# include <linux/pkt_sched.h>
# include <linux/rtnetlink.h>

static const unsigned int hdrlen = sizeof(struct tcmsg);

static void
print_u_field(const char *name, uint64_t val, bool first)
{
	jsonl_field_open(name, first);
	jsonl_unsigned(val);
	jsonl_field_close();
}

static void
print_d_field(const char *name, int64_t val)
{
	jsonl_field_open(name, false);
	jsonl_decimal(val);
	jsonl_field_close();
}

static void
init_tcmsg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = RTM_GETQDISC,
		.nlmsg_flags = NLM_F_DUMP
	);

	struct tcmsg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct tcmsg, msg,
		.tcm_family = AF_UNIX,
		.tcm_ifindex = ifindex_lo()
	);

	struct nlattr *const nla = NLMSG_ATTR(nlh, sizeof(*msg));
	SET_STRUCT(struct nlattr, nla,
		.nla_len = msg_len - NLMSG_SPACE(hdrlen),
		.nla_type = TCA_STAB
	);
}

static void
print_tcmsg(const unsigned int msg_len)
{
	static const struct strval32 nlmsg_flags[] = {
		{ ARG_STR(NLM_F_DUMP) }
	};

	printf("{");
	jsonl_nlmsghdr_vals(msg_len, ARG_STR(RTM_GETQDISC), NULL,
			    NLM_F_DUMP, nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	jsonl_struct_open();
	jsonl_field_open("tcm_family", true);
	jsonl_const(AF_UNIX, true, "AF_UNIX", NULL);
	jsonl_field_close();
	jsonl_field_open("tcm_ifindex", false);
	jsonl_ifindex(ifindex_lo(), "lo");
	jsonl_field_close();
	print_u_field("tcm_handle", 0, false);
	print_u_field("tcm_parent", 0, false);
	print_u_field("tcm_info", 0, false);
	jsonl_struct_close();
	printf("}");
	jsonl_nlattr_nest_open(msg_len - NLMSG_SPACE(hdrlen),
			       TCA_STAB, "TCA_STAB", NULL);
}

static void
print_uint16(const uint16_t *p, size_t idx)
{
	jsonl_unsigned(*p);
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
				   NLA_HDRLEN + sizeof(struct tc_sizespec));

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	static const struct tc_sizespec s = {
		.cell_log = 0xab,
		.size_log = 0xcd,
		.cell_align = 0xefab,
		.overhead = 0xcdadeefa,
		.linklayer = 0xefbaafeb,
		.mpu = 0xfebfaefb,
		.mtu = 0xacdbefab,
		.tsize = 0xbdeaabed
	};
	JSONL_NESTED_NLATTR_OBJECT(fd, nlh0, hdrlen,
				   init_tcmsg, print_tcmsg,
				   TCA_STAB_BASE, pattern, s,
				   jsonl_struct_open();
				   print_u_field("cell_log", s.cell_log, true);
				   print_u_field("size_log", s.size_log, false);
				   print_d_field("cell_align", s.cell_align);
				   print_d_field("overhead", s.overhead);
				   print_u_field("linklayer", s.linklayer,
						 false);
				   print_u_field("mpu", s.mpu, false);
				   print_u_field("mtu", s.mtu, false);
				   print_u_field("tsize", s.tsize, false);
				   jsonl_struct_close());

	uint16_t data[2] = { 0xacbd, 0xefba };
	JSONL_NESTED_NLATTR_ARRAY(fd, nlh0, hdrlen,
				  init_tcmsg, print_tcmsg,
				  TCA_STAB_DATA, pattern, data, print_uint16);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
