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
	JSONL_NLATTR_(fd, nlh0, hdrlen,
		      jsonl_nl_init_tcmsg, jsonl_nl_print_tcmsg,
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
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen,
			    jsonl_nl_init_tcmsg, jsonl_nl_print_tcmsg,
			    TCA_STATS, pattern, buf,
			    jsonl_struct_open();
			    jsonl_field_unsigned("bytes", true, st.bytes);
			    jsonl_field_unsigned("packets", false, st.packets);
			    jsonl_field_unsigned("drops", false, st.drops);
			    jsonl_field_unsigned("overlimits", false,
						 st.overlimits);
			    jsonl_field_unsigned("bps", false, st.bps);
			    jsonl_field_unsigned("pps", false, st.pps);
			    jsonl_field_unsigned("qlen", false, st.qlen);
			    jsonl_field_unsigned("backlog", false, st.backlog);
			    jsonl_struct_close());

	static const struct tc_estimator est = {
		.interval = 0xcd,
		.ewma_log = 0xab
	};
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen,
			    jsonl_nl_init_tcmsg, jsonl_nl_print_tcmsg,
			    TCA_RATE, pattern, est,
			    jsonl_struct_open();
			    jsonl_field_decimal("interval", true, est.interval);
			    jsonl_field_unsigned("ewma_log", false,
						 est.ewma_log);
			    jsonl_struct_close());

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
