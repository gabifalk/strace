/*
 * Check JSONL output of RTA_* netlink attributes of rtmsg.
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
# include <arpa/inet.h>
# include <netinet/in.h>
# include <linux/ip.h>

# define LWTUNNEL_ENCAP_NONE 0

static const unsigned int hdrlen = sizeof(struct rtmsg);

static uint8_t rtm_family = AF_UNIX;
static const char *rtm_family_str = "AF_UNIX";

static void
init_rtmsg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = RTM_GETROUTE,
		.nlmsg_flags = NLM_F_DUMP
	);

	struct rtmsg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct rtmsg, msg,
		.rtm_family = rtm_family,
		.rtm_tos = IPTOS_LOWDELAY,
		.rtm_table = RT_TABLE_DEFAULT,
		.rtm_protocol = RTPROT_KERNEL,
		.rtm_scope = RT_SCOPE_UNIVERSE,
		.rtm_type = RTN_LOCAL,
		.rtm_flags = RTM_F_NOTIFY
	);
}

static void
print_rtmsg(const unsigned int msg_len)
{
	static const struct strval32 rtm_tos[] = {
		{ ARG_STR(IPTOS_LOWDELAY) }
	};
	static const struct strval32 rtm_flags[] = {
		{ ARG_STR(RTM_F_NOTIFY) }
	};

	jsonl_nlmsghdr_dump_open(msg_len, ARG_STR(RTM_GETROUTE));
	jsonl_struct_open();
	jsonl_field_const("rtm_family", true, rtm_family, rtm_family_str, NULL);
	jsonl_field_unsigned("rtm_dst_len", false, 0);
	jsonl_field_unsigned("rtm_src_len", false, 0);
	jsonl_field_flags("rtm_tos", false, IPTOS_LOWDELAY, rtm_tos,
			  ARRAY_SIZE(rtm_tos), NULL);
	jsonl_field_const("rtm_table", false, ARG_STR(RT_TABLE_DEFAULT), NULL);
	jsonl_field_const("rtm_protocol", false, ARG_STR(RTPROT_KERNEL), NULL);
	jsonl_field_const("rtm_scope", false, ARG_STR(RT_SCOPE_UNIVERSE), NULL);
	jsonl_field_const("rtm_type", false, ARG_STR(RTN_LOCAL), NULL);
	jsonl_field_flags("rtm_flags", false, RTM_F_NOTIFY, rtm_flags,
			  ARRAY_SIZE(rtm_flags), NULL);
	jsonl_struct_close();
	printf("}");
}

static void
set_family(uint8_t af, const char *af_str)
{
	rtm_family = af;
	rtm_family_str = af_str;
}

static void
print_rtnexthop(const struct rtnexthop *const nh)
{
	static const struct strval32 rtnh_flags[] = {
		{ ARG_STR(RTNH_F_DEAD) },
		{ ARG_STR(RTNH_F_PERVASIVE) },
		{ ARG_STR(RTNH_F_ONLINK) },
		{ ARG_STR(RTNH_F_OFFLOAD) },
		{ ARG_STR(RTNH_F_LINKDOWN) },
		{ ARG_STR(RTNH_F_UNRESOLVED) },
		{ ARG_STR(RTNH_F_TRAP) },
	};

	jsonl_struct_open();
	jsonl_field_unsigned("rtnh_len", true, nh->rtnh_len);
	jsonl_field_flags("rtnh_flags", false, nh->rtnh_flags, rtnh_flags,
			  ARRAY_SIZE(rtnh_flags), "RTNH_F_???");
	jsonl_field_unsigned("rtnh_hops", false, nh->rtnh_hops);
	jsonl_field_ifindex("rtnh_ifindex", false, nh->rtnh_ifindex, "lo");
	jsonl_struct_close();
}

static void
print_rtvia(unsigned int af, const char *af_str, const char *addr)
{
	jsonl_struct_open();
	jsonl_field_const("rtvia_family", true, af, af_str, NULL);
	if (addr) {
		jsonl_field_open("rtvia_addr", false);
		jsonl_ip_addr(af, af_str, addr);
		jsonl_field_close();
	}
	jsonl_struct_close();
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	const unsigned int nla_type = 0xffff & NLA_TYPE_MASK;
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen), NLA_HDRLEN + 256);

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	JSONL_NLATTR_(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
		      nla_type, NULL, "RTA_???",
		      4, pattern, 4,
		      jsonl_bytes_value(pattern, 4, false));

	JSONL_NLATTR(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
		     RTA_DST, 4, pattern, 4,
		     jsonl_bytes_value(pattern, 4, false));

# define MAX_ADDR_SZ 35
	static const struct {
		uint8_t af;
		const char *af_str;
		uint8_t addr[MAX_ADDR_SZ];
		const char *str;
		uint32_t len;
	} addrs[] = {
		{ ARG_STR(AF_UNIX), { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 },
		  NULL, 10 },
		{ ARG_STR(AF_UNIX),
		  { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, [MAX_ADDR_SZ - 1] = 0xea },
		  NULL, MAX_ADDR_SZ },
		{ ARG_STR(AF_INET), { 0xde, 0xca, 0xff, 0xed },
		  "222.202.255.237", 4 },
		{ ARG_STR(AF_INET6), { 0xfa, 0xce, 0xbe, 0xef, [15] = 0xda },
		  "face:beef::da", 16 },
	};
	static const struct strval32 addr_attrs[] = {
		{ ARG_STR(RTA_DST) },
		{ ARG_STR(RTA_SRC) },
		{ ARG_STR(RTA_GATEWAY) },
		{ ARG_STR(RTA_PREFSRC) },
		{ ARG_STR(RTA_NEWDST) },
	};
	for (size_t i = 0; i < ARRAY_SIZE(addrs); i++) {
		const uint32_t len = addrs[i].len;

		set_family(addrs[i].af, addrs[i].af_str);
		for (size_t j = 0; j < ARRAY_SIZE(addr_attrs); j++) {
			JSONL_NLATTR_(fd, nlh0, hdrlen,
				      init_rtmsg, print_rtmsg,
				      addr_attrs[j].val, addr_attrs[j].str,
				      NULL,
				      len - 1, addrs[i].addr, len - 1,
				      jsonl_bytes_value(addrs[i].addr,
					MIN(len - 1, DEFAULT_STRLEN),
					len - 1 > DEFAULT_STRLEN));
			JSONL_NLATTR_(fd, nlh0, hdrlen,
				      init_rtmsg, print_rtmsg,
				      addr_attrs[j].val, addr_attrs[j].str,
				      NULL,
				      len, addrs[i].addr, len,
				      if (addrs[i].str)
					jsonl_ip_addr(addrs[i].af,
						      addrs[i].af_str,
						      addrs[i].str);
				      else
					jsonl_bytes_value(addrs[i].addr,
						MIN(len, DEFAULT_STRLEN),
						len > DEFAULT_STRLEN));
		}
	}
	set_family(ARG_STR(AF_UNIX));

	const uint32_t ifindex = ifindex_lo();
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			    RTA_OIF, pattern, ifindex,
			    jsonl_ifindex(ifindex, "lo"));

	const uint32_t rt_class_id = RT_TABLE_DEFAULT;
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			    RTA_TABLE, pattern, rt_class_id,
			    jsonl_const(RT_TABLE_DEFAULT, true,
					"RT_TABLE_DEFAULT", NULL));

	struct nlattr nla = {
		.nla_type = RTAX_LOCK,
		.nla_len = sizeof(nla)
	};
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			    RTA_METRICS, pattern, nla,
			    jsonl_nlattr_hdr(nla.nla_len, ARG_STR(RTAX_LOCK),
					     NULL));

	/* A struct rtnexthop that is too short to have attributes */
	struct rtnexthop nh = {
		.rtnh_len = sizeof(nh) - 1,
		.rtnh_flags = RTNH_F_DEAD,
		.rtnh_hops = 0xab,
		.rtnh_ifindex = ifindex_lo()
	};
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			    RTA_MULTIPATH, pattern, nh,
			    jsonl_array_open();
			    printf("{");
			    print_rtnexthop(&nh);
			    printf("}");
			    jsonl_array_close());

	/* A struct rtnexthop followed by an attribute without a payload */
	char buf[RTNH_ALIGN(sizeof(nh)) + sizeof(nla)];
	nh.rtnh_len = sizeof(buf);
	nla.nla_type = RTA_DST;
	memcpy(buf, &nh, sizeof(nh));
	memcpy(buf + RTNH_ALIGN(sizeof(nh)), &nla, sizeof(nla));
	JSONL_NLATTR(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
		     RTA_MULTIPATH, sizeof(buf), buf, sizeof(buf),
		     jsonl_array_open();
		     printf("{");
		     print_rtnexthop(&nh);
		     printf("}, {");
		     jsonl_nlattr_hdr(nla.nla_len, ARG_STR(RTA_DST), NULL);
		     printf("}");
		     jsonl_array_close());

	/* Two nexthops, each followed by an attribute with a payload */
	static const struct in_addr gw_inet_addr = {
		.s_addr = BE32(0xdeadbeef)
	};
	static const uint8_t via_inet6_addr[16] = {
		0xde, 0xad, 0xfa, 0xce, 0xbe, 0xef, 0xca, 0xfe,
		0xfe, 0xed, 0xba, 0x5e, 0x00, 0x00, 0xfa, 0xde };
	static const struct rtvia rtvia = { .rtvia_family = AF_INET6 };
	const unsigned int gw_nla_len =
		NLMSG_ALIGN(sizeof(nla)) + NLMSG_ALIGN(sizeof(gw_inet_addr));
	const unsigned int via_nla_len =
		NLMSG_ALIGN(sizeof(nla))
		+ NLMSG_ALIGN(offsetof(struct rtvia, rtvia_addr)
			      + sizeof(via_inet6_addr));
	char buf2[2 * RTNH_ALIGN(sizeof(nh)) + gw_nla_len + via_nla_len];
	char *pos = buf2;
	struct rtnexthop nh_gw = nh;
	struct rtnexthop nh_via = nh;

	nh_gw.rtnh_len = RTNH_ALIGN(sizeof(nh)) + gw_nla_len;
	nh_gw.rtnh_flags = 0xc0;
	nla.nla_type = RTA_GATEWAY;
	nla.nla_len = gw_nla_len;
	memcpy(pos, &nh_gw, sizeof(nh_gw));
	pos += sizeof(nh_gw);
	memcpy(pos, &nla, sizeof(nla));
	pos += sizeof(nla);
	memcpy(pos, &gw_inet_addr, sizeof(gw_inet_addr));
	pos += sizeof(gw_inet_addr);

	nh_via.rtnh_len = RTNH_ALIGN(sizeof(nh)) + via_nla_len;
	nh_via.rtnh_flags = 0xc0;
	nla.nla_type = RTA_VIA;
	nla.nla_len = via_nla_len;
	memcpy(pos, &nh_via, sizeof(nh_via));
	pos += sizeof(nh_via);
	memcpy(pos, &nla, sizeof(nla));
	pos += sizeof(nla);
	memcpy(pos, &rtvia, sizeof(rtvia));
	pos += sizeof(rtvia);
	memcpy(pos, &via_inet6_addr, sizeof(via_inet6_addr));

	set_family(ARG_STR(AF_INET));
	JSONL_NLATTR(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
		     RTA_MULTIPATH, sizeof(buf2), buf2, sizeof(buf2),
		     jsonl_array_open();
		     printf("{");
		     jsonl_array_open();
		     printf("{");
		     print_rtnexthop(&nh_gw);
		     printf("}, {");
		     jsonl_nlattr_open(gw_nla_len, ARG_STR(RTA_GATEWAY), NULL);
		     jsonl_ip_addr(ARG_STR(AF_INET), "222.173.190.239");
		     jsonl_nlattr_close();
		     printf("}");
		     jsonl_array_close();
		     printf("}, {");
		     jsonl_array_open();
		     printf("{");
		     print_rtnexthop(&nh_via);
		     printf("}, {");
		     jsonl_nlattr_open(via_nla_len, ARG_STR(RTA_VIA), NULL);
		     print_rtvia(ARG_STR(AF_INET6),
				 "dead:face:beef:cafe:feed:ba5e:0:fade");
		     jsonl_nlattr_close();
		     printf("}");
		     jsonl_array_close();
		     printf("}");
		     jsonl_array_close());
	set_family(ARG_STR(AF_UNIX));

	static const struct rta_cacheinfo ci = {
		.rta_clntref = 0xabcdefab,
		.rta_lastuse = 0xbdadaedc,
		.rta_expires = 0xcdadebad,
		.rta_error = 0xdaedadeb,
		.rta_used = 0xedfabdad,
		.rta_id = 0xfeadbcda,
		.rta_ts = 0xacdbaded,
		.rta_tsage = 0xbadeadef
	};
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			    RTA_CACHEINFO, pattern, ci,
			    jsonl_struct_open();
			    jsonl_field_unsigned("rta_clntref", true,
						 ci.rta_clntref);
			    jsonl_field_unsigned("rta_lastuse", false,
						 ci.rta_lastuse);
			    jsonl_field_unsigned("rta_expires", false,
						 (uint32_t) ci.rta_expires);
			    jsonl_field_unsigned("rta_error", false,
						 ci.rta_error);
			    jsonl_field_unsigned("rta_used", false,
						 ci.rta_used);
			    jsonl_field_hex("rta_id", false, ci.rta_id);
			    jsonl_field_unsigned("rta_ts", false, ci.rta_ts);
			    jsonl_field_unsigned("rta_tsage", false,
						 ci.rta_tsage);
			    jsonl_struct_close());

	static const struct rta_mfc_stats mfcs = {
		.mfcs_packets = 0xadcdedfdadefadcd,
		.mfcs_bytes = 0xbaedadedcdedadbd,
		.mfcs_wrong_if = 0xcddeabeedaedabfa
	};
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			    RTA_MFC_STATS, pattern, mfcs,
			    jsonl_struct_open();
			    jsonl_field_unsigned("mfcs_packets", true,
						 mfcs.mfcs_packets);
			    jsonl_field_unsigned("mfcs_bytes", false,
						 mfcs.mfcs_bytes);
			    jsonl_field_unsigned("mfcs_wrong_if", false,
						 mfcs.mfcs_wrong_if);
			    jsonl_struct_close());

	/* A struct rtvia without an address and with an address */
	static const struct rtvia via = {
		.rtvia_family = AF_INET
	};
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			    RTA_VIA, pattern, via,
			    print_rtvia(ARG_STR(AF_INET), NULL));

	static const char address4[] = "12.34.56.78";
	struct in_addr a4 = {
		.s_addr = inet_addr(address4)
	};
	char rtviabuf[sizeof(via) + sizeof(a4)];
	memcpy(rtviabuf, &via, sizeof(via));
	memcpy(rtviabuf + sizeof(via), &a4, sizeof(a4));

	JSONL_NLATTR(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
		     RTA_VIA, sizeof(rtviabuf), rtviabuf, sizeof(rtviabuf),
		     print_rtvia(ARG_STR(AF_INET), address4));

	const uint16_t encap_type = LWTUNNEL_ENCAP_NONE;
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			    RTA_ENCAP_TYPE, pattern, encap_type,
			    jsonl_const(LWTUNNEL_ENCAP_NONE, true,
					"LWTUNNEL_ENCAP_NONE", NULL));

	/* RTA_DEL_REASON */
	static const struct strval32 del_reasons[] = {
		{ 0, "RT_DEL_REASON_UNSPEC" },
		{ 0x1, "RT_DEL_REASON_EXPIRED" },
		{ 0x2, "RT_DEL_REASON_RA_WITHDRAWN" },
		{ 0x3, NULL },
		{ 0xdeadbeef, NULL },
	};
	for (size_t i = 0; i < ARRAY_SIZE(del_reasons); i++) {
		JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
				    RTA_DEL_REASON, pattern,
				    del_reasons[i].val,
				    jsonl_const(del_reasons[i].val, true,
						del_reasons[i].str,
						"RT_DEL_REASON_???"));
		/* The value is followed by data that cannot be read. */
		JSONL_NLATTR(fd, nlh0, hdrlen, init_rtmsg, print_rtmsg,
			     RTA_DEL_REASON, sizeof(del_reasons[i].val) + 4,
			     &del_reasons[i].val, sizeof(del_reasons[i].val),
			     jsonl_const(del_reasons[i].val, true,
					 del_reasons[i].str,
					 "RT_DEL_REASON_???"));
	}

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
