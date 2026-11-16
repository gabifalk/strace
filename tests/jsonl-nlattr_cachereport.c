/*
 * Check JSONL output of netlink attributes of RTM_NEWCACHEREPORT messages.
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

# include <linux/ip.h>
# include <linux/mroute.h>
# include <linux/mroute6.h>

# include "xlat.h"
# include "xlat/addrfams.h"

static uint8_t af;
static const char *af_str;

static void
init_rtgen(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = RTM_NEWCACHEREPORT,
		.nlmsg_flags = NLM_F_EXCL|NLM_F_APPEND,
	);

	struct rtgenmsg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct rtgenmsg, msg,
		.rtgen_family = af,
	);
}

static void
print_rtgenmsg(const unsigned int family, const char *const sym)
{
	jsonl_struct_open();
	jsonl_field_open("rtgen_family", true);
	jsonl_const(family, true, sym, "RTNL_FAMILY_???");
	jsonl_field_close();
	jsonl_struct_close();
}

static void
print_rtgen(const unsigned int msg_len)
{
	static const struct strval32 nlmsg_flags[] = {
		{ ARG_STR(NLM_F_EXCL) },
		{ ARG_STR(NLM_F_APPEND) },
	};

	printf("{");
	jsonl_nlmsghdr_vals(msg_len, ARG_STR(RTM_NEWCACHEREPORT), NULL,
			    NLM_F_EXCL|NLM_F_APPEND,
			    nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	print_rtgenmsg(af, af_str);
	printf("}");
}

static const char *
addrfam_name(const unsigned int family)
{
	for (size_t i = 0; i < addrfams->size; ++i) {
		if (addrfams->data[i].str && addrfams->data[i].val == family)
			return addrfams->data[i].str;
	}

	return NULL;
}

int
main(int argc, char *argv[])
{
	static const uint8_t unknown_msg[] = { 0xab, 0xac, 0xdb, 0xcd };

	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);

	const unsigned int hdrlen = sizeof(struct rtgenmsg);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen), NLA_HDRLEN + 16);

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);


	/* Families without a cache report decoder */
	static const uint8_t skip_afs[] = {
		RTNL_FAMILY_IPMR, RTNL_FAMILY_IP6MR
	};
	struct {
		struct rtgenmsg ATTRIBUTE_ALIGNED(NLMSG_ALIGNTO) hdr;
		struct {
			char str[sizeof(unknown_msg)];
		} ATTRIBUTE_ALIGNED(NLMSG_ALIGNTO) payload;
	} buf;
	memset(&buf, 0, sizeof(buf));
	memcpy(buf.payload.str, unknown_msg, sizeof(unknown_msg));
	size_t pos = 0;
	for (size_t i = 0; i < 256; i++) {
		if (pos < ARRAY_SIZE(skip_afs) && skip_afs[pos] == i) {
			pos += 1;
			continue;
		}

		buf.hdr.rtgen_family = i;
		JSONL_NETLINK(fd, nlh0, RTM_NEWCACHEREPORT, NLM_F_REPLACE,
			      sizeof(buf), &buf, sizeof(buf),
			      print_rtgenmsg(i, addrfam_name(i));
			      jsonl_nlmsg_next();
			      jsonl_bytes_value(unknown_msg,
						sizeof(unknown_msg), false));
	}


	/* RTNL_FAMILY_IPMR */
	af = RTNL_FAMILY_IPMR;
	af_str = "RTNL_FAMILY_IPMR";

	/* RTNL_FAMILY_IPMR: unknown, undecoded */
	static const struct strval16 unk_attrs[] = {
		{ ARG_STR(IPMRA_CREPORT_UNSPEC) },
		{ ARG_STR(IPMRA_CREPORT_PKT) },
		{ 0x7, NULL },
		{ 0x1ead, NULL },
	};

	for (size_t i = 0; i < ARRAY_SIZE(unk_attrs); i++) {
		JSONL_NLATTR_(fd, nlh0, hdrlen, init_rtgen, print_rtgen,
			      unk_attrs[i].val, unk_attrs[i].str,
			      "IPMRA_CREPORT_???",
			      16, pattern, 16,
			      jsonl_bytes_value(pattern, 16, false));
	}

	/* RTNL_FAMILY_IPMR: IPMRA_CREPORT_MSGTYPE */
	static const struct strval8 mr_msg_types[] = {
		{ 0, NULL },
		{ 0x1, "IGMPMSG_NOCACHE" },
		{ 0x2, "IGMPMSG_WRONGVIF" },
		{ 0x3, "IGMPMSG_WHOLEPKT" },
		{ 0x4, "IGMPMSG_WRVIFWHOLE" },
		{ 0x5, NULL },
		{ 0xca, NULL },
	};
	for (size_t i = 0; i < ARRAY_SIZE(mr_msg_types); i++) {
		JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtgen, print_rtgen,
				    IPMRA_CREPORT_MSGTYPE,
				    pattern, mr_msg_types[i].val,
				    jsonl_const(mr_msg_types[i].val, true,
						mr_msg_types[i].str,
						"IGMPMSG_???"));
	}

	/* RTNL_FAMILY_IPMR: u32 */
	static const struct strval16 u32_mr_attrs[] = {
		{ ARG_STR(IPMRA_CREPORT_VIF_ID) },
		{ ARG_STR(IPMRA_CREPORT_TABLE) },
	};
	void *nlh_u32 = midtail_alloc(NLMSG_SPACE(hdrlen),
				      NLA_HDRLEN + sizeof(uint32_t));
	for (size_t i = 0; i < ARRAY_SIZE(u32_mr_attrs); i++) {
		jsonl_check_u32_nlattr(fd, nlh_u32, hdrlen,
				       init_rtgen, print_rtgen,
				       u32_mr_attrs[i].val,
				       u32_mr_attrs[i].str, NULL,
				       pattern, 0);
	}

	/* RTNL_FAMILY_IPMR: in_addr */
	static const struct strval16 in_addr_attrs[] = {
		{ ARG_STR(IPMRA_CREPORT_SRC_ADDR) },
		{ ARG_STR(IPMRA_CREPORT_DST_ADDR) },
	};
	static const uint32_t ipv4_addr = BE32(0xdeadface);
	for (size_t i = 0; i < ARRAY_SIZE(in_addr_attrs); i++) {
		JSONL_NLATTR_OBJECT_(fd, nlh0, hdrlen, init_rtgen, print_rtgen,
				     in_addr_attrs[i].val,
				     in_addr_attrs[i].str, NULL,
				     pattern, ipv4_addr,
				     jsonl_ip_addr(ARG_STR(AF_INET),
						   "222.173.250.206"));
	}


	/* RTNL_FAMILY_IP6MR */
	af = RTNL_FAMILY_IP6MR;
	af_str = "RTNL_FAMILY_IP6MR";

	/* RTNL_FAMILY_IP6MR: unknown, undecoded */
	static const struct strval16 unk6_attrs[] = {
		{ ARG_STR(IP6MRA_CREPORT_UNSPEC) },
		{ ARG_STR(IP6MRA_CREPORT_PKT) },
		{ 0x6, NULL },
		{ 0x1ead, NULL },
	};

	for (size_t i = 0; i < ARRAY_SIZE(unk6_attrs); i++) {
		JSONL_NLATTR_(fd, nlh0, hdrlen, init_rtgen, print_rtgen,
			      unk6_attrs[i].val, unk6_attrs[i].str,
			      "IP6MRA_CREPORT_???",
			      16, pattern, 16,
			      jsonl_bytes_value(pattern, 16, false));
	}

	/* RTNL_FAMILY_IP6MR: IP6MRA_CREPORT_MSGTYPE */
	static const struct strval8 mr6_msg_types[] = {
		{ 0, NULL },
		{ 0x1, "MRT6MSG_NOCACHE" },
		{ 0x2, "MRT6MSG_WRONGMIF" },
		{ 0x3, "MRT6MSG_WHOLEPKT" },
		{ 0x4, NULL },
		{ 0xca, NULL },
	};
	for (size_t i = 0; i < ARRAY_SIZE(mr6_msg_types); i++) {
		JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_rtgen, print_rtgen,
				    IP6MRA_CREPORT_MSGTYPE,
				    pattern, mr6_msg_types[i].val,
				    jsonl_const(mr6_msg_types[i].val, true,
						mr6_msg_types[i].str,
						"MRT6MSG_???"));
	}

	/* RTNL_FAMILY_IP6MR: u32 */
	jsonl_check_u32_nlattr(fd, nlh_u32, hdrlen, init_rtgen, print_rtgen,
			       ARG_STR(IP6MRA_CREPORT_MIF_ID), NULL,
			       pattern, 0);

	/* RTNL_FAMILY_IP6MR: in6_addr */
	static const struct strval16 in6_addr_attrs[] = {
		{ ARG_STR(IP6MRA_CREPORT_SRC_ADDR) },
		{ ARG_STR(IP6MRA_CREPORT_DST_ADDR) },
	};
	static const uint8_t ipv6_addr[16] = {
		0xba, 0xdc, 0x0d, 0xed, 0xfa, 0xce, 0xbe, 0xef,
		0xde, 0xca, 0xfe, 0xed, 0xde, 0xad, 0xfe, 0xed,
	};
	for (size_t i = 0; i < ARRAY_SIZE(in6_addr_attrs); i++) {
		JSONL_NLATTR_OBJECT_(fd, nlh0, hdrlen, init_rtgen, print_rtgen,
				     in6_addr_attrs[i].val,
				     in6_addr_attrs[i].str, NULL,
				     pattern, ipv6_addr,
				     jsonl_ip_addr(ARG_STR(AF_INET6),
						   "badc:ded:face:beef"
						   ":deca:feed:dead:feed"));
	}

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
