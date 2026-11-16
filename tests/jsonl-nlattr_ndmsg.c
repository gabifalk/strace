/*
 * Check JSONL output of NDA_* netlink attributes of ndmsg.
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
# include <netinet/in.h>
# include <linux/neighbour.h>

static const unsigned int hdrlen = sizeof(struct ndmsg);

static uint8_t ndm_family = AF_UNIX;
static const char *ndm_family_str = "AF_UNIX";

static void
init_ndmsg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = RTM_GETNEIGH,
		.nlmsg_flags = NLM_F_DUMP
	);

	struct ndmsg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct ndmsg, msg,
		.ndm_family = ndm_family,
		.ndm_ifindex = ifindex_lo(),
		.ndm_state = NUD_PERMANENT,
		.ndm_flags = NTF_PROXY,
		.ndm_type = RTN_UNSPEC
	);
}

static void
print_ndmsg(const unsigned int msg_len)
{
	static const struct strval32 ndm_state[] = {
		{ ARG_STR(NUD_PERMANENT) }
	};
	static const struct strval32 ndm_flags[] = {
		{ ARG_STR(NTF_PROXY) }
	};

	jsonl_nlmsghdr_dump_open(msg_len, ARG_STR(RTM_GETNEIGH));
	jsonl_struct_open();
	jsonl_field_const("ndm_family", true, ndm_family, ndm_family_str, NULL);
	jsonl_field_ifindex("ndm_ifindex", false, ifindex_lo(), "lo");
	jsonl_field_flags("ndm_state", false, NUD_PERMANENT, ndm_state,
			  ARRAY_SIZE(ndm_state), NULL);
	jsonl_field_flags("ndm_flags", false, NTF_PROXY, ndm_flags,
			  ARRAY_SIZE(ndm_flags), NULL);
	jsonl_field_const("ndm_type", false, RTN_UNSPEC, "RTN_UNSPEC", NULL);
	jsonl_struct_close();
	printf("}");
}

static void
init_ndmsg_nfea(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	init_ndmsg(nlh, msg_len);

	struct nlattr *nla = NLMSG_ATTR(nlh, hdrlen);
	SET_STRUCT(struct nlattr, nla,
		.nla_len = msg_len - NLMSG_SPACE(hdrlen),
		.nla_type = NDA_FDB_EXT_ATTRS,
	);
}

static void
print_ndmsg_nfea(const unsigned int msg_len)
{
	print_ndmsg(msg_len);
	jsonl_nlattr_nest_open(msg_len - NLMSG_SPACE(hdrlen),
			       ARG_STR(NDA_FDB_EXT_ATTRS), NULL);
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
				   2 * NLA_HDRLEN
				   + sizeof(struct nda_cacheinfo));

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	const unsigned int nla_type = 0xffff & NLA_TYPE_MASK;
	JSONL_NLATTR_(fd, nlh0, hdrlen, init_ndmsg, print_ndmsg,
		      nla_type, NULL, "NDA_???",
		      4, pattern, 4,
		      jsonl_bytes_value(pattern, 4, false));

	/* The address family of the message has no address decoder. */
	JSONL_NLATTR(fd, nlh0, hdrlen, init_ndmsg, print_ndmsg,
		     NDA_DST, 4, pattern, 4,
		     jsonl_bytes_value(pattern, 4, false));

	static const struct nda_cacheinfo ci = {
		.ndm_confirmed = 0xabcdedad,
		.ndm_used = 0xbcdaedad,
		.ndm_updated = 0xcdbadeda,
		.ndm_refcnt = 0xdeadbeda
	};

	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_ndmsg, print_ndmsg,
			    NDA_CACHEINFO, pattern, ci,
			    jsonl_struct_open();
			    jsonl_field_unsigned("ndm_confirmed", true,
						 ci.ndm_confirmed);
			    jsonl_field_unsigned("ndm_used", false,
						 ci.ndm_used);
			    jsonl_field_unsigned("ndm_updated", false,
						 ci.ndm_updated);
			    jsonl_field_unsigned("ndm_refcnt", false,
						 ci.ndm_refcnt);
			    jsonl_struct_close());

	const uint16_t port = 0xabcd;
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_ndmsg, print_ndmsg,
			    NDA_PORT, pattern, port,
			    jsonl_unsigned(ntohs(port));
			    jsonl_byteorder_big(sizeof(port)));

	/* Unlike NDA_PORT, NDA_VLAN is in host byte order. */
	const uint16_t vlan = 0xabcd;
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_ndmsg, print_ndmsg,
			    NDA_VLAN, pattern, vlan,
			    jsonl_unsigned(vlan));

	static const uint8_t mac[6] = {
		0xf8, 0xc2, 0x49, 0x13, 0x57, 0xbd
	};
	JSONL_NLATTR(fd, nlh0, hdrlen, init_ndmsg, print_ndmsg,
		     NDA_LLADDR, sizeof(mac) - 1, mac, sizeof(mac) - 1,
		     jsonl_mac(mac, sizeof(mac) - 1));

	JSONL_NLATTR(fd, nlh0, hdrlen, init_ndmsg, print_ndmsg,
		     NDA_LLADDR, sizeof(mac), mac, sizeof(mac),
		     jsonl_mac(mac, sizeof(mac)));

	const uint32_t ifindex = ifindex_lo();
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_ndmsg, print_ndmsg,
			    NDA_IFINDEX, pattern, ifindex,
			    jsonl_ifindex(ifindex, "lo"));
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_ndmsg, print_ndmsg,
			    NDA_MASTER, pattern, ifindex,
			    jsonl_ifindex(ifindex, "lo"));

	/* u32 attrs */
	static const struct strval32 u32_attrs[] = {
		{ ARG_STR(NDA_PROBES) },
		{ ARG_STR(NDA_VNI) },
		{ ARG_STR(NDA_LINK_NETNSID) },
		{ ARG_STR(NDA_SRC_VNI) },
		{ ARG_STR(NDA_NH_ID) },
	};
	void *nlh_u32 = midtail_alloc(NLMSG_SPACE(hdrlen), sizeof(uint32_t));

	for (size_t i = 0; i < ARRAY_SIZE(u32_attrs); i++) {
		jsonl_check_u32_nlattr(fd, nlh_u32, hdrlen,
				       init_ndmsg, print_ndmsg,
				       u32_attrs[i].val, u32_attrs[i].str, NULL,
				       pattern, 0);
	}

	/* NDA_FDB_EXT_ATTRS: unknown, undecoded */
	static const struct strval32 nfea_unk_attrs[] = {
		{ ARG_STR(NFEA_UNSPEC) },
		{ ARG_STR(NFEA_DONT_REFRESH) },
		{ 0x3, NULL },
		{ 0x1ace, NULL },
	};
	static const uint32_t dummy = BE_LE(0xbadc0ded, 0xed0ddcba);

	for (size_t i = 0; i < ARRAY_SIZE(nfea_unk_attrs); i++) {
		JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
				     init_ndmsg_nfea, print_ndmsg_nfea,
				     nfea_unk_attrs[i].val,
				     nfea_unk_attrs[i].str, "NFEA_???",
				     sizeof(dummy), &dummy, sizeof(dummy), 1,
				     jsonl_bytes_value(&dummy, sizeof(dummy),
						       false));
	}

	/* NDA_FDB_EXT_ATTRS: NFEA_ACTIVITY_NOTIFY */
	static const struct strval32 fdb_notify_flags[] = {
		{ ARG_STR(FDB_NOTIFY_BIT) },
		{ ARG_STR(FDB_NOTIFY_INACTIVE_BIT) },
	};
	static const uint8_t fan_flags[] = { 0, 0x1, 0xef, 0xfc };

	for (size_t i = 0; i < ARRAY_SIZE(fan_flags); i++) {
		JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
				     init_ndmsg_nfea, print_ndmsg_nfea,
				     NFEA_ACTIVITY_NOTIFY,
				     "NFEA_ACTIVITY_NOTIFY", NULL,
				     1, &fan_flags[i], 1, 1,
				     jsonl_flags(fan_flags[i], fdb_notify_flags,
						 ARRAY_SIZE(fdb_notify_flags),
						 "FDB_NOTIFY_???"));
	}

	/* NDA_FLAGS_EXT */
	static const struct strval32 ntf_ext_flags[] = {
		{ ARG_STR(NTF_EXT_MANAGED) },
		{ ARG_STR(NTF_EXT_LOCKED) },
		{ ARG_STR(NTF_EXT_EXT_VALIDATED) },
	};
	static const uint32_t ntfe_flags[] = {
		0, 0x1, 0x2, 0x4, 0xdeadbeef, 0xfeedcaf8
	};

	for (size_t i = 0; i < ARRAY_SIZE(ntfe_flags); i++) {
		JSONL_NLATTR(fd, nlh0, hdrlen, init_ndmsg, print_ndmsg,
			     NDA_FLAGS_EXT, 4, &ntfe_flags[i], 4,
			     jsonl_flags(ntfe_flags[i], ntf_ext_flags,
					 ARRAY_SIZE(ntf_ext_flags),
					 "NTF_EXT_???"));
	}

	/* NDA_NDM_STATE_MASK */
	static const struct strval32 nud_states[] = {
		{ ARG_STR(NUD_NONE) },
		{ ARG_STR(NUD_INCOMPLETE) },
		{ ARG_STR(NUD_REACHABLE) },
		{ ARG_STR(NUD_STALE) },
		{ ARG_STR(NUD_DELAY) },
		{ ARG_STR(NUD_PROBE) },
		{ ARG_STR(NUD_FAILED) },
		{ ARG_STR(NUD_NOARP) },
		{ ARG_STR(NUD_PERMANENT) },
	};
	static const uint16_t states_flags[] = { 0, 0x1, 0xabed, 0xff00 };

	for (size_t i = 0; i < ARRAY_SIZE(states_flags); i++) {
		JSONL_NLATTR(fd, nlh0, hdrlen, init_ndmsg, print_ndmsg,
			     NDA_NDM_STATE_MASK, 2, &states_flags[i], 2,
			     jsonl_flags(states_flags[i], nud_states,
					 ARRAY_SIZE(nud_states), "NUD_???"));
	}

	/* NDA_NDM_FLAGS_MASK */
	static const struct strval32 ntf_flags[] = {
		{ ARG_STR(NTF_USE) },
		{ ARG_STR(NTF_SELF) },
		{ ARG_STR(NTF_MASTER) },
		{ ARG_STR(NTF_PROXY) },
		{ ARG_STR(NTF_EXT_LEARNED) },
		{ ARG_STR(NTF_OFFLOADED) },
		{ ARG_STR(NTF_STICKY) },
		{ ARG_STR(NTF_ROUTER) },
	};
	static const uint8_t ndm_flags[] = { 0, 0x1, 0xbe };

	for (size_t i = 0; i < ARRAY_SIZE(ndm_flags); i++) {
		JSONL_NLATTR(fd, nlh0, hdrlen, init_ndmsg, print_ndmsg,
			     NDA_NDM_FLAGS_MASK, 1, &ndm_flags[i], 1,
			     jsonl_flags(ndm_flags[i], ntf_flags,
					 ARRAY_SIZE(ntf_flags), "NTF_???"));
	}

	/* NDA_DST of the address families that have an address decoder */
	static const char ip4_str[] = "1.2.3.4";
	static const char ip6_str[] = "2001:db8::abcd";
	struct in_addr ip4;
	struct in6_addr ip6;

	if (inet_pton(AF_INET, ip4_str, &ip4) != 1
	    || inet_pton(AF_INET6, ip6_str, &ip6) != 1)
		perror_msg_and_fail("inet_pton");

	ndm_family = AF_INET;
	ndm_family_str = "AF_INET";
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_ndmsg, print_ndmsg,
			    NDA_DST, pattern, ip4,
			    jsonl_ip_addr(ARG_STR(AF_INET), ip4_str));

	ndm_family = AF_INET6;
	ndm_family_str = "AF_INET6";
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_ndmsg, print_ndmsg,
			    NDA_DST, pattern, ip6,
			    jsonl_ip_addr(ARG_STR(AF_INET6), ip6_str));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
