/*
 * Check JSONL output of FRA_* netlink attributes of fib_rule_hdr.
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

# include <arpa/inet.h>
# include <linux/fib_rules.h>
# include <linux/in.h>
# include <linux/ip.h>

static void
init_rtmsg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = RTM_GETRULE,
		.nlmsg_flags = NLM_F_DUMP
	);

	struct rtmsg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct rtmsg, msg,
		.rtm_family = AF_UNIX,
		.rtm_tos = IPTOS_LOWDELAY,
		.rtm_table = RT_TABLE_UNSPEC,
		.rtm_type = FR_ACT_TO_TBL,
		.rtm_flags = FIB_RULE_INVERT
	);
}

static void
print_rtmsg(const unsigned int msg_len)
{
	jsonl_nlmsghdr_dump_open(msg_len, ARG_STR(RTM_GETRULE));
	jsonl_struct_open();
	jsonl_field_const("family", true, ARG_STR(AF_UNIX), NULL);
	jsonl_field_unsigned("dst_len", false, 0);
	jsonl_field_unsigned("src_len", false, 0);
	jsonl_field_flag("tos", false, ARG_STR(IPTOS_LOWDELAY));
	jsonl_field_const("table", false, ARG_STR(RT_TABLE_UNSPEC), NULL);
	jsonl_field_const("action", false, ARG_STR(FR_ACT_TO_TBL), NULL);
	jsonl_field_flag("flags", false, ARG_STR(FIB_RULE_INVERT));
	jsonl_struct_close();
	printf("}");
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	const unsigned int hdrlen = sizeof(struct rtmsg);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen), NLA_HDRLEN + 8);

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	const unsigned int nla_type = 0xffff & NLA_TYPE_MASK;
	JSONL_NLATTR_(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
		      nla_type, NULL, "FRA_???",
		      4, pattern, 4,
		      jsonl_bytes_value(pattern, 4, false));

	/* The address family of the message has no address decoder. */
	JSONL_NLATTR(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
		     FRA_DST, 4, pattern, 4,
		     jsonl_bytes_value(pattern, 4, false));

	const uint32_t table_id = RT_TABLE_DEFAULT;
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			    FRA_TABLE, pattern, table_id,
			    jsonl_const(table_id, true, "RT_TABLE_DEFAULT",
					NULL));

	static const struct fib_rule_uid_range range = {
		.start = 0xabcdedad,
		.end = 0xbcdeadba
	};
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			    FRA_UID_RANGE, pattern, range,
			    jsonl_struct_open();
			    jsonl_field_unsigned("start", true, range.start);
			    jsonl_field_unsigned("end", false, range.end);
			    jsonl_struct_close());

	const unsigned int flow_label = 0xedabedaf;
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			    FRA_FLOWLABEL, pattern, flow_label,
			    jsonl_unsigned(ntohl(flow_label));
			    jsonl_byteorder_big(sizeof(flow_label)));
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			    FRA_FLOWLABEL_MASK, pattern, flow_label,
			    jsonl_unsigned(ntohl(flow_label));
			    jsonl_byteorder_big(sizeof(flow_label)));

# if defined HAVE_BE64TOH || defined be64toh
	const uint64_t tun_id = 0xabcdcdbeedabadef;
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			    FRA_TUN_ID, pattern, tun_id,
			    jsonl_unsigned(be64toh(tun_id));
			    jsonl_byteorder_big(sizeof(tun_id)));
# endif

	uint8_t proto;

	static const struct strval8 proto_args[] = {
		{ ARG_STR(RTPROT_UNSPEC) },
		{ 5, NULL },
		{ 17, "RTPROT_MROUTED" },
		{ 42, "RTPROT_BABEL" },
		{ 43, NULL },
		{ 0xde, NULL },
	};

	for (size_t i = 0; i < ARRAY_SIZE(proto_args); i++) {
		proto = proto_args[i].val;
		JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
				    FRA_PROTOCOL, pattern, proto,
				    jsonl_const(proto, true, proto_args[i].str,
						"RTPROT_???"));
	}

	static const struct strval8 ipproto_args[] = {
		{ ARG_STR(IPPROTO_TCP) },
		{ 254, NULL },
		{ ARG_STR(IPPROTO_RAW) },
	};

	for (size_t i = 0; i < ARRAY_SIZE(ipproto_args); i++) {
		proto = ipproto_args[i].val;
		JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
				    FRA_IP_PROTO, pattern, proto,
				    jsonl_const(proto, true,
						ipproto_args[i].str,
						"IPPROTO_???"));
	}

	static const struct fib_rule_port_range prange = {
		.start = 0xabcd,
		.end = 0xfeed,
	};
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			    FRA_SPORT_RANGE, pattern, prange,
			    jsonl_struct_open();
			    jsonl_field_unsigned("start", true, prange.start);
			    jsonl_field_unsigned("end", false, prange.end);
			    jsonl_struct_close());
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			    FRA_DPORT_RANGE, pattern, prange,
			    jsonl_struct_open();
			    jsonl_field_unsigned("start", true, prange.start);
			    jsonl_field_unsigned("end", false, prange.end);
			    jsonl_struct_close());

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
