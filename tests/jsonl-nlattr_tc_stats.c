/*
 * Check JSONL output of TCA_STATS_* netlink attributes of tcmsg.
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

# include <stddef.h>
# include <linux/gen_stats.h>
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
		.nla_type = TCA_STATS2
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
			       TCA_STATS2, "TCA_STATS2", NULL);
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen), NLA_HDRLEN + 8 * 5);

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	static const struct gnet_stats_basic sb = {
		.bytes = 0xabcdebdafefeadeb,
		.packets = 0xbdcdeabf
	};
	char buf[offsetofend(struct gnet_stats_basic, packets)];
	memcpy(buf, &sb, sizeof(buf));
	JSONL_NESTED_NLATTR_OBJECT(fd, nlh0, hdrlen,
				   init_tcmsg, print_tcmsg,
				   TCA_STATS_BASIC, pattern, buf,
				   jsonl_struct_open();
				   print_u_field("bytes", sb.bytes, true);
				   print_u_field("packets", sb.packets, false);
				   jsonl_struct_close());

	static const struct gnet_stats_rate_est est = {
		.bps = 0xebcdaebd,
		.pps = 0xabdceade,
	};
	JSONL_NESTED_NLATTR_OBJECT(fd, nlh0, hdrlen,
				   init_tcmsg, print_tcmsg,
				   TCA_STATS_RATE_EST, pattern, est,
				   jsonl_struct_open();
				   print_u_field("bps", est.bps, true);
				   print_u_field("pps", est.pps, false);
				   jsonl_struct_close());

	static const struct gnet_stats_queue qstats = {
		.qlen = 0xabcdeabd,
		.backlog = 0xbcdaebad,
		.drops = 0xcdbeaedb,
		.requeues = 0xdebaefab,
		.overlimits = 0xefaebade
	};
	JSONL_NESTED_NLATTR_OBJECT(fd, nlh0, hdrlen,
				   init_tcmsg, print_tcmsg,
				   TCA_STATS_QUEUE, pattern, qstats,
				   jsonl_struct_open();
				   print_u_field("qlen", qstats.qlen, true);
				   print_u_field("backlog", qstats.backlog,
						 false);
				   print_u_field("drops", qstats.drops, false);
				   print_u_field("requeues", qstats.requeues,
						 false);
				   print_u_field("overlimits",
						 qstats.overlimits, false);
				   jsonl_struct_close());

	static const struct gnet_stats_rate_est64 est64 = {
		.bps = 0xacbdcdefafecaebf,
		.pps = 0xcdabeabdfeabceaf
	};
	JSONL_NESTED_NLATTR_OBJECT(fd, nlh0, hdrlen,
				   init_tcmsg, print_tcmsg,
				   TCA_STATS_RATE_EST64, pattern, est64,
				   jsonl_struct_open();
				   print_u_field("bps", est64.bps, true);
				   print_u_field("pps", est64.pps, false);
				   jsonl_struct_close());

	static const uint64_t pkt64 = 0xdeadc0defacefeedULL;
	JSONL_NESTED_NLATTR_OBJECT(fd, nlh0, hdrlen,
				   init_tcmsg, print_tcmsg,
				   TCA_STATS_PKT64, pattern, pkt64,
				   jsonl_unsigned(pkt64));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
