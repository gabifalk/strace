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
print_u_field(const char *name, unsigned int val, bool first)
{
	jsonl_field_open(name, first);
	jsonl_unsigned(val);
	jsonl_field_close();
}

static void
print_const_field(const char *name, unsigned int val, const char *sym,
		  bool first)
{
	jsonl_field_open(name, first);
	jsonl_const(val, true, sym, NULL);
	jsonl_field_close();
}

static void
print_flag_field(const char *name, unsigned int val, const char *sym)
{
	const struct strval32 flag = { val, sym };

	jsonl_field_open(name, false);
	jsonl_flags(val, &flag, 1, NULL);
	jsonl_field_close();
}

static void
print_rtmsg(const unsigned int msg_len)
{
	static const struct strval32 nlmsg_flags[] = {
		{ ARG_STR(NLM_F_DUMP) }
	};

	printf("{");
	jsonl_nlmsghdr_vals(msg_len, ARG_STR(RTM_GETRULE), NULL,
			    NLM_F_DUMP, nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	jsonl_struct_open();
	print_const_field("family", ARG_STR(AF_UNIX), true);
	print_u_field("dst_len", 0, false);
	print_u_field("src_len", 0, false);
	print_flag_field("tos", ARG_STR(IPTOS_LOWDELAY));
	print_const_field("table", ARG_STR(RT_TABLE_UNSPEC), false);
	print_const_field("action", ARG_STR(FR_ACT_TO_TBL), false);
	print_flag_field("flags", ARG_STR(FIB_RULE_INVERT));
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
			    print_u_field("start", range.start, true);
			    print_u_field("end", range.end, false);
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
			    print_u_field("start", prange.start, true);
			    print_u_field("end", prange.end, false);
			    jsonl_struct_close());
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			    FRA_DPORT_RANGE, pattern, prange,
			    jsonl_struct_open();
			    print_u_field("start", prange.start, true);
			    print_u_field("end", prange.end, false);
			    jsonl_struct_close());

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
