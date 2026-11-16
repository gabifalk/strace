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
init_tcmsg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	jsonl_nl_init_tcmsg(nlh, msg_len);

	struct nlattr *const nla = NLMSG_ATTR(nlh, hdrlen);
	SET_STRUCT(struct nlattr, nla,
		.nla_len = msg_len - NLMSG_SPACE(hdrlen),
		.nla_type = TCA_STAB
	);
}

static void
print_tcmsg(const unsigned int msg_len)
{
	jsonl_nl_print_tcmsg(msg_len);
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
				   jsonl_field_unsigned("cell_log", true,
							s.cell_log);
				   jsonl_field_unsigned("size_log", false,
							s.size_log);
				   jsonl_field_decimal("cell_align", false,
						       s.cell_align);
				   jsonl_field_decimal("overhead", false,
						       s.overhead);
				   jsonl_field_unsigned("linklayer", false,
							s.linklayer);
				   jsonl_field_unsigned("mpu", false, s.mpu);
				   jsonl_field_unsigned("mtu", false, s.mtu);
				   jsonl_field_unsigned("tsize", false,
							s.tsize);
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
