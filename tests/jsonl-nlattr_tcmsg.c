/*
 * Check JSONL output of TCA_* netlink attributes of tcmsg.
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
# include <linux/pkt_sched.h>
# include <linux/rtnetlink.h>

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
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	const unsigned int hdrlen = sizeof(struct tcmsg);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
				   NLA_HDRLEN + sizeof(struct tc_stats));

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	const unsigned int nla_type = 0xffff & NLA_TYPE_MASK;
	JSONL_NLATTR_(fd, nlh0, hdrlen, init_tcmsg, print_tcmsg,
		      nla_type, NULL, "TCA_???",
		      4, pattern, 4,
		      jsonl_bytes_value(pattern, 4, false));

	static const struct tc_stats st = {
		.bytes = 0xabcdcdbefeadefac,
		.packets = 0xbcdeaefd,
		.drops = 0xcdedafed,
		.overlimits = 0xdcdbefad,
		.bps = 0xefaebfad,
		.pps = 0xfefbaedb,
		.qlen = 0xabcdefab,
		.backlog = 0xbdeabeab
	};
	char buf[offsetofend(struct tc_stats, backlog)];
	memcpy(buf, &st, sizeof(buf));
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_tcmsg, print_tcmsg,
			    TCA_STATS, pattern, buf,
			    jsonl_struct_open();
			    print_u_field("bytes", st.bytes, true);
			    print_u_field("packets", st.packets, false);
			    print_u_field("drops", st.drops, false);
			    print_u_field("overlimits", st.overlimits, false);
			    print_u_field("bps", st.bps, false);
			    print_u_field("pps", st.pps, false);
			    print_u_field("qlen", st.qlen, false);
			    print_u_field("backlog", st.backlog, false);
			    jsonl_struct_close());

	static const struct tc_estimator est = {
		.interval = 0xcd,
		.ewma_log = 0xab
	};
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_tcmsg, print_tcmsg,
			    TCA_RATE, pattern, est,
			    jsonl_struct_open();
			    jsonl_field_open("interval", true);
			    jsonl_decimal(est.interval);
			    jsonl_field_close();
			    print_u_field("ewma_log", est.ewma_log, false);
			    jsonl_struct_close());

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
