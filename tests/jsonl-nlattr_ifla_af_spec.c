/*
 * Check JSONL output of IFLA_AF_SPEC netlink attributes.
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

# include <linux/if.h>
# include <linux/if_arp.h>
# include <linux/if_bridge.h>
# include <linux/if_link.h>

# include "xlat.h"
# include "xlat/addrfams.h"

# define XLAT_MACROS_ONLY
#  include "xlat/rtnl_ifla_af_spec_inet_attrs.h"
#  include "xlat/rtnl_ifla_af_spec_inet6_attrs.h"
# undef XLAT_MACROS_ONLY

static uint8_t msg_af = AF_UNIX;
static const char *msg_af_str = "AF_UNIX";

# define IFLA_AF msg_af
# define IFLA_AF_STR msg_af_str
# define IFLA_ATTR IFLA_AF_SPEC

/* The JSONL counterpart of nlattr_ifla.h. */
# ifndef IFLA_AF
#  define IFLA_AF AF_UNIX
# endif
# ifndef IFLA_AF_STR
#  define IFLA_AF_STR "AF_UNIX"
# endif

static const unsigned int hdrlen = sizeof(struct ifinfomsg);

static void
init_ifinfomsg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = RTM_GETLINK,
		.nlmsg_flags = NLM_F_DUMP
	);

	struct ifinfomsg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct ifinfomsg, msg,
		.ifi_family = IFLA_AF,
		.ifi_type = ARPHRD_LOOPBACK,
		.ifi_index = ifindex_lo(),
		.ifi_flags = IFF_UP,
	);

	struct nlattr *const nla = NLMSG_ATTR(nlh, sizeof(*msg));
	SET_STRUCT(struct nlattr, nla,
		.nla_len = msg_len - NLMSG_SPACE(hdrlen),
		.nla_type = IFLA_ATTR
	);
}

/* The netlink header and the struct ifinfomsg. */
static void
print_ifinfomsg_hdr(const unsigned int msg_len)
{
	static const struct strval32 nlmsg_flags[] = {
		{ ARG_STR(NLM_F_DUMP) }
	};
	static const struct strval32 ifi_flags[] = {
		{ ARG_STR(IFF_UP) }
	};

	printf("{");
	jsonl_nlmsghdr_vals(msg_len, ARG_STR(RTM_GETLINK), NULL,
			    NLM_F_DUMP, nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	jsonl_struct_open();
	jsonl_field_open("ifi_family", true);
	jsonl_const(IFLA_AF, true, IFLA_AF_STR, "AF_???");
	jsonl_field_close();
	jsonl_field_open("ifi_type", false);
	jsonl_const(ARPHRD_LOOPBACK, true, "ARPHRD_LOOPBACK", NULL);
	jsonl_field_close();
	jsonl_field_open("ifi_index", false);
	jsonl_ifindex(ifindex_lo(), "lo");
	jsonl_field_close();
	jsonl_field_open("ifi_flags", false);
	jsonl_flags(IFF_UP, ifi_flags, ARRAY_SIZE(ifi_flags), NULL);
	jsonl_field_close();
	jsonl_field_open("ifi_change", false);
	jsonl_hex(0);
	jsonl_field_close();
	jsonl_struct_close();
	printf("}");
}

/* Likewise, followed by the enclosing IFLA_ATTR attribute. */
static void
print_ifinfomsg(const unsigned int msg_len)
{
	print_ifinfomsg_hdr(msg_len);
	jsonl_nlattr_nest_open(msg_len - NLMSG_SPACE(hdrlen),
			       IFLA_ATTR, STRINGIFY_VAL(IFLA_ATTR), NULL);
}

/* The JSONL counterpart of nlattr_ifla_af_inet6.h. */
static void
print_indexed_elem(uint64_t idx, const char *const *syms, size_t nsyms,
		   const char *pattern)
{
	printf("\"index\": {");
	jsonl_const(idx, false, idx < nsyms ? syms[idx] : NULL,
		    idx < nsyms ? NULL : pattern);
	printf("}, ");
}

static void
print_inet6_conf_val(const uint32_t *val, size_t idx)
{
	static const char *const syms[] = {
		"DEVCONF_FORWARDING",
		"DEVCONF_HOPLIMIT",
	};

	print_indexed_elem(idx, syms, ARRAY_SIZE(syms), "DEVCONF_???");
	jsonl_decimal((int32_t) *val);
}

static void
print_inet6_stats_val(const uint64_t *val, size_t idx)
{
	static const char *const syms[] = {
		"IPSTATS_MIB_NUM",
		"IPSTATS_MIB_INPKTS",
	};

	print_indexed_elem(idx, syms, ARRAY_SIZE(syms), "IPSTATS_MIB_???");
	jsonl_unsigned(*val);
}

static void
print_icmp6_stats_val(const uint64_t *val, size_t idx)
{
	static const char *const syms[] = {
		"ICMP6_MIB_NUM",
		"ICMP6_MIB_INMSGS",
		"ICMP6_MIB_INERRORS",
		"ICMP6_MIB_OUTMSGS",
		"ICMP6_MIB_OUTERRORS",
		"ICMP6_MIB_CSUMERRORS",
	};

	print_indexed_elem(idx, syms, ARRAY_SIZE(syms), "ICMP6_MIB_???");
	jsonl_unsigned(*val);
}

static void
print_inet6_u32_field(const char *name, uint32_t val, bool first)
{
	jsonl_field_open(name, first);
	jsonl_unsigned(val);
	jsonl_field_close();
}

static void
check_ifla_af_inet6(int fd, void *nlh0, unsigned int hdrlen,
		    void (*init_msg)(struct nlmsghdr *, unsigned int),
		    void (*print_msg)(unsigned int),
		    void *pattern, unsigned int depth)
{
	static const uint8_t unknown_msg[] = { 0xab, 0xac, 0xdb, 0xcd };

	/* Undecoded, unknown */
	JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen, init_msg, print_msg,
				       0, "IFLA_INET6_UNSPEC", NULL,
				       pattern, unknown_msg, jsonl_nl_bytes,
				       depth,
				       jsonl_nl_bytes(unknown_msg,
						      sizeof(unknown_msg)));
	JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen, init_msg, print_msg,
				       10, NULL, "IFLA_INET6_???",
				       pattern, unknown_msg, jsonl_nl_bytes,
				       depth,
				       jsonl_nl_bytes(unknown_msg,
						      sizeof(unknown_msg)));

	/* IFLA_INET6_FLAGS */
	static const struct strval32 inet6_if_flags[] = {
		{ 0x10, "IF_RS_SENT" },
		{ 0x20, "IF_RA_RCVD" },
		{ 0x40, "IF_RA_MANAGED" },
		{ 0x80, "IF_RA_OTHERCONF" },
		{ 0x80000000, "IF_READY" },
	};
	static const uint32_t inet6_flags[] = {
		0xf, 0x10, 0xc0, 0xdeadc0de
	};

	for (size_t i = 0; i < ARRAY_SIZE(inet6_flags); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
					       init_msg, print_msg,
					       1, "IFLA_INET6_FLAGS", NULL,
					       pattern, inet6_flags[i],
					       jsonl_nl_bytes, depth,
					       jsonl_flags(inet6_flags[i],
						   inet6_if_flags,
						   ARRAY_SIZE(inet6_if_flags),
						   "IF_???"));
	}

	/* IFLA_INET6_CONF */
	uint32_t inet6_conf_vals[] = { 0xdeadc0de, 0xda7aface };
	JSONL_NESTED_NLATTR_ARRAY_EX_(fd, nlh0, hdrlen, init_msg, print_msg,
				      IFLA_INET6_CONF, "IFLA_INET6_CONF", NULL,
				      pattern, inet6_conf_vals, depth,
				      print_inet6_conf_val);

	/* IFLA_INET6_STATS */
	uint64_t inet6_stats_vals[] = {
		0xdeadc0deda7aface, 0xdec0deedbadc0ded
	};
	JSONL_NESTED_NLATTR_ARRAY_EX_(fd, nlh0, hdrlen, init_msg, print_msg,
				      IFLA_INET6_STATS, "IFLA_INET6_STATS",
				      NULL, pattern, inet6_stats_vals, depth,
				      print_inet6_stats_val);

	/* IFLA_INET6_MCAST */
	JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen, init_msg, print_msg,
				       4, "IFLA_INET6_MCAST", NULL,
				       pattern, unknown_msg, jsonl_nl_bytes,
				       depth,
				       jsonl_nl_bytes(unknown_msg,
						      sizeof(unknown_msg)));

	/* IFLA_INET6_CACHEINFO */
	static const struct ifla_cacheinfo ci = {
		0xbadc0ded, 0xfacebeef, 0xdecafeed, 0xdeadfeed,
	};
	JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen, init_msg, print_msg,
				       5, "IFLA_INET6_CACHEINFO", NULL,
				       pattern, ci, jsonl_nl_bytes, depth,
				       jsonl_struct_open();
				       print_inet6_u32_field("max_reasm_len",
							     ci.max_reasm_len,
							     true);
				       print_inet6_u32_field("tstamp",
							     ci.tstamp, false);
				       print_inet6_u32_field("reachable_time",
							     ci.reachable_time,
							     false);
				       print_inet6_u32_field("retrans_time",
							     ci.retrans_time,
							     false);
				       jsonl_struct_close());

	/* IFLA_INET6_ICMP6STATS */
	uint64_t icmp6_stats_vals[] = {
		0xdeadc0deda7aface, 0xdec0deedbadc0ded, 0xfacebeefdeadfeed,
		0xdeadc0deda7afacd, 0xdec0deedbadc0dee, 0xfacebeefdeadfeef,
		0xdeadc0deda7afacc
	};
	JSONL_NESTED_NLATTR_ARRAY_EX_(fd, nlh0, hdrlen, init_msg, print_msg,
				      IFLA_INET6_ICMP6STATS,
				      "IFLA_INET6_ICMP6STATS", NULL,
				      pattern, icmp6_stats_vals, depth,
				      print_icmp6_stats_val);

	/* IFLA_INET6_TOKEN */
	uint8_t inet6_addr[16] = {
		0xba, 0xdc, 0x0d, 0xed, 0xfa, 0xce, 0xbe, 0xef,
		0xde, 0xca, 0xfe, 0xed, 0xde, 0xad, 0xfe, 0xed,
	};
	JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen, init_msg, print_msg,
				       7, "IFLA_INET6_TOKEN", NULL,
				       pattern, inet6_addr, jsonl_nl_bytes,
				       depth,
				       jsonl_ip_addr(ARG_STR(AF_INET6),
						     "badc:ded:face:beef"
						     ":deca:feed:dead:feed"));

	/* IFLA_INET6_ADDR_GEN_MODE */
	static const struct strval32 agms[] = {
		{ 0, "IN6_ADDR_GEN_MODE_EUI64" },
		{ 0x3, "IN6_ADDR_GEN_MODE_RANDOM" },
		{ 0x4, NULL },
		{ 0xff, NULL },
	};

	for (size_t i = 0; i < ARRAY_SIZE(agms); i++) {
		const uint8_t mode = agms[i].val;

		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
					       init_msg, print_msg,
					       8, "IFLA_INET6_ADDR_GEN_MODE",
					       NULL, pattern, mode,
					       jsonl_nl_bytes, depth,
					       jsonl_const(mode, true,
						   agms[i].str,
						   "IN6_ADDR_GEN_MODE_???"));
	}

	/* IFLA_INET6_RA_MTU */
	static const uint32_t ra_mtu = 0xdeadc0de;
	JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen, init_msg, print_msg,
				       9, "IFLA_INET6_RA_MTU", NULL,
				       pattern, ra_mtu, jsonl_nl_bytes, depth,
				       jsonl_unsigned(ra_mtu));
}

# define AF_SPEC_FUNCS(family_)						\
	static void							\
	init_##family_##_msg(struct nlmsghdr *const nlh,		\
			     const unsigned int msg_len)		\
	{								\
		init_ifinfomsg(nlh, msg_len);				\
									\
		struct nlattr *nla = NLMSG_ATTR(nlh, hdrlen);		\
		nla += 1;						\
		SET_STRUCT(struct nlattr, nla,				\
			.nla_len = msg_len - NLMSG_SPACE(hdrlen)	\
				  - NLA_HDRLEN,				\
			.nla_type = family_,				\
		);							\
	}								\
									\
	static void							\
	print_##family_##_msg(const unsigned int msg_len)		\
	{								\
		print_ifinfomsg(msg_len);				\
		jsonl_nlattr_nest_open(msg_len - NLMSG_SPACE(hdrlen)	\
				       - NLA_HDRLEN,			\
				       family_, #family_, NULL);	\
	}								\
	/* end of AF_SPEC_FUNCS definition */

AF_SPEC_FUNCS(AF_INET)
AF_SPEC_FUNCS(AF_INET6)
AF_SPEC_FUNCS(AF_MCTP)

AF_SPEC_FUNCS(IFLA_BRIDGE_VLAN_TUNNEL_INFO)

/* The name of the address family, or NULL if it has no name. */
static const char *
af_name(const unsigned int family)
{
	const char *const str = sprintxval_abbrev(addrfams, family, "AF_???");

	return strncmp(str, "AF_", 3) ? NULL : str;
}

static void
print_inet_conf_val(const uint32_t *val, size_t idx)
{
	/* The names of these constants are expressions. */
	static const char *const syms[] = {
		"IPV4_DEVCONF_FORWARDING-1",
		"IPV4_DEVCONF_MC_FORWARDING-1",
	};

	print_indexed_elem(idx, syms, ARRAY_SIZE(syms), "IPV4_DEVCONF_???");
	jsonl_decimal((int32_t) *val);
}

static const struct strval32 bridge_vlan_flags[] = {
	{ ARG_STR(BRIDGE_VLAN_INFO_MASTER) },
	{ ARG_STR(BRIDGE_VLAN_INFO_PVID) },
	{ ARG_STR(BRIDGE_VLAN_INFO_UNTAGGED) },
	{ ARG_STR(BRIDGE_VLAN_INFO_RANGE_BEGIN) },
	{ ARG_STR(BRIDGE_VLAN_INFO_RANGE_END) },
	{ ARG_STR(BRIDGE_VLAN_INFO_BRENTRY) },
	{ ARG_STR(BRIDGE_VLAN_INFO_ONLY_OPTS) },
};

static void
print_bridge_vlan_info(const struct bridge_vlan_info *const bvi)
{
	jsonl_struct_open();
	jsonl_field_open("flags", true);
	jsonl_flags(bvi->flags, bridge_vlan_flags,
		    ARRAY_SIZE(bridge_vlan_flags), "BRIDGE_VLAN_INFO_???");
	jsonl_field_close();
	jsonl_field_open("vid", false);
	jsonl_unsigned(bvi->vid);
	jsonl_field_close();
	jsonl_struct_close();
}

int
main(int argc, char *argv[])
{
	static const uint8_t unknown_msg[] = { 0xab, 0xac, 0xdb, 0xcd };

	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen), 3 * NLA_HDRLEN + 256);

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	/*
	 * unknown AF_*: all of them in a message of the AF_UNIX family,
	 * a few of them in messages of other families.
	 */
	static const uint8_t skip_afs[] = { AF_INET, AF_INET6, AF_MCTP };
	for (unsigned int j = 0; j < 64; j++) {
		if (j == AF_BRIDGE)
			continue;

		msg_af = j;
		msg_af_str = af_name(j);
		size_t pos = 0;

		for (unsigned int i = 0; i < 64; i++) {
			if (pos < ARRAY_SIZE(skip_afs) && skip_afs[pos] == i) {
				pos += 1;
				continue;
			}
			if (j != AF_UNIX
			    && i != AF_UNSPEC && i != AF_BRIDGE && i != 63)
				continue;

			JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
				init_ifinfomsg, print_ifinfomsg,
				i, af_name(i), "AF_???",
				pattern, unknown_msg, jsonl_nl_bytes, 1,
				jsonl_nl_bytes(unknown_msg,
					       sizeof(unknown_msg)));
		}
	}

	msg_af = AF_UNIX;
	msg_af_str = "AF_UNIX";

	/* AF_INET */
	JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
				       init_AF_INET_msg, print_AF_INET_msg,
				       0, "IFLA_INET_UNSPEC", NULL, pattern,
				       unknown_msg, jsonl_nl_bytes, 2,
				       jsonl_nl_bytes(unknown_msg,
						      sizeof(unknown_msg)));
	JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
				       init_AF_INET_msg, print_AF_INET_msg,
				       2, NULL, "IFLA_INET_???", pattern,
				       unknown_msg, jsonl_nl_bytes, 2,
				       jsonl_nl_bytes(unknown_msg,
						      sizeof(unknown_msg)));

	/* AF_INET: IFLA_INET_CONF */
	uint32_t inet_conf_vals[] = { 0xdeadc0de, 0xda7aface };
	JSONL_NESTED_NLATTR_ARRAY_EX(fd, nlh0, hdrlen,
				     init_AF_INET_msg, print_AF_INET_msg,
				     IFLA_INET_CONF, pattern,
				     inet_conf_vals, 2, print_inet_conf_val);

	/* AF_BRIDGE */
	msg_af = AF_BRIDGE;
	msg_af_str = "AF_BRIDGE";

	/* AF_BRIDGE: unknown, unimplemented */
	static const struct strval32 unk_attrs[] = {
		{ ARG_STR(IFLA_BRIDGE_MRP) },
		{ ARG_STR(IFLA_BRIDGE_CFM) },
		{ ARG_STR(IFLA_BRIDGE_MST) },
		{ 0x7, NULL },
		{ 0xbad, NULL },
	};
	for (size_t i = 0; i < ARRAY_SIZE(unk_attrs); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
					       init_ifinfomsg, print_ifinfomsg,
					       unk_attrs[i].val,
					       unk_attrs[i].str,
					       "IFLA_BRIDGE_???",
					       pattern, unknown_msg,
					       jsonl_nl_bytes, 1,
					       jsonl_nl_bytes(unknown_msg,
						       sizeof(unknown_msg)));
	}

	/* AF_BRIDGE: IFLA_BRIDGE_FLAGS */
	static const struct strval32 bridge_fl[] = {
		{ ARG_STR(BRIDGE_FLAGS_MASTER) },
		{ ARG_STR(BRIDGE_FLAGS_SELF) },
	};
	static const uint16_t bridge_flags[] = {
		0,
		BRIDGE_FLAGS_MASTER,
		BRIDGE_FLAGS_SELF,
		BRIDGE_FLAGS_MASTER | BRIDGE_FLAGS_SELF,
		0x4,
		0xcafe,
		0x7eac,
	};
	for (size_t i = 0; i < ARRAY_SIZE(bridge_flags); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
					       init_ifinfomsg, print_ifinfomsg,
					       IFLA_BRIDGE_FLAGS,
					       "IFLA_BRIDGE_FLAGS", NULL,
					       pattern, bridge_flags[i],
					       jsonl_nl_bytes, 1,
					       jsonl_flags(bridge_flags[i],
						   bridge_fl,
						   ARRAY_SIZE(bridge_fl),
						   "BRIDGE_FLAGS_???"));
	}

	/* AF_BRIDGE: IFLA_BRIDGE_MODE */
	static const struct strval32 bridge_modes[] = {
		{ ARG_STR(BRIDGE_MODE_VEB) },
		{ ARG_STR(BRIDGE_MODE_VEPA) },
		{ 0x2, NULL },
		{ 0x3, NULL },
		{ 0xcafe, NULL },
		{ 0xfffe, NULL },
		{ ARG_STR(BRIDGE_MODE_UNDEF) },
	};
	for (size_t i = 0; i < ARRAY_SIZE(bridge_modes); i++) {
		const uint16_t mode = bridge_modes[i].val;

		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
					       init_ifinfomsg, print_ifinfomsg,
					       IFLA_BRIDGE_MODE,
					       "IFLA_BRIDGE_MODE", NULL,
					       pattern, mode,
					       jsonl_nl_bytes, 1,
					       jsonl_const(mode, true,
							   bridge_modes[i].str,
							   "BRIDGE_MODE_???"));
	}

	/* AF_BRIDGE: IFLA_BRIDGE_VLAN_INFO */
	static const struct bridge_vlan_info bridge_vis[] = {
		{ 0, 0 },
		{ 1, 1 },
		{ 0x69, 0xface },
		{ 0xef80, 0xfeed },
		{ 0xcafe, 0xdead },
	};
	char bvi_buf[12];

	fill_memory_ex(bvi_buf, sizeof(bvi_buf), 'z', 0x80);

	for (size_t i = 0; i < ARRAY_SIZE(bridge_vis); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
					       init_ifinfomsg, print_ifinfomsg,
					       IFLA_BRIDGE_VLAN_INFO,
					       "IFLA_BRIDGE_VLAN_INFO", NULL,
					       pattern, bridge_vis[i],
					       jsonl_nl_bytes, 1,
					       print_bridge_vlan_info(
							&bridge_vis[i]));

		memcpy(bvi_buf, &bridge_vis[i], sizeof(bridge_vis[i]));
		JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
				     init_ifinfomsg, print_ifinfomsg,
				     IFLA_BRIDGE_VLAN_INFO,
				     "IFLA_BRIDGE_VLAN_INFO", NULL,
				     sizeof(bvi_buf), bvi_buf, sizeof(bvi_buf),
				     1,
				     print_bridge_vlan_info(&bridge_vis[i]);
				     printf("}, {");
				     jsonl_nl_bytes(bvi_buf
						    + sizeof(bridge_vis[i]),
						    sizeof(bvi_buf)
						    - sizeof(bridge_vis[i])));
	}

	/* AF_BRIDGE: IFLA_BRIDGE_TUNNEL_INFO: unknown, undecoded */
	static const struct strval32 unk_bti_attrs[] = {
		{ ARG_STR(IFLA_BRIDGE_VLAN_TUNNEL_UNSPEC) },
		{ 0x4, NULL },
		{ 0xbad, NULL },
	};
	for (size_t i = 0; i < ARRAY_SIZE(unk_bti_attrs); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
				init_IFLA_BRIDGE_VLAN_TUNNEL_INFO_msg,
				print_IFLA_BRIDGE_VLAN_TUNNEL_INFO_msg,
				unk_bti_attrs[i].val, unk_bti_attrs[i].str,
				"IFLA_BRIDGE_VLAN_TUNNEL_???",
				pattern, unknown_msg, jsonl_nl_bytes, 2,
				jsonl_nl_bytes(unknown_msg,
					       sizeof(unknown_msg)));
	}

	/* AF_BRIDGE: IFLA_BRIDGE_TUNNEL_INFO: u32 attrs */
	void *nlh_u32 = midtail_alloc(NLMSG_SPACE(hdrlen),
				      NLA_HDRLEN + sizeof(uint32_t));
	jsonl_check_u32_nlattr(fd, nlh_u32, hdrlen,
			       init_IFLA_BRIDGE_VLAN_TUNNEL_INFO_msg,
			       print_IFLA_BRIDGE_VLAN_TUNNEL_INFO_msg,
			       ARG_STR(IFLA_BRIDGE_VLAN_TUNNEL_ID), NULL,
			       pattern, 2);

	/* AF_BRIDGE: IFLA_BRIDGE_TUNNEL_INFO: u16 attrs */
	void *nlh_u16 = midtail_alloc(NLMSG_SPACE(hdrlen),
				      NLA_HDRLEN + sizeof(uint16_t));
	jsonl_check_u16_nlattr(fd, nlh_u16, hdrlen,
			       init_IFLA_BRIDGE_VLAN_TUNNEL_INFO_msg,
			       print_IFLA_BRIDGE_VLAN_TUNNEL_INFO_msg,
			       ARG_STR(IFLA_BRIDGE_VLAN_TUNNEL_VID), NULL,
			       pattern, 2);

	/* AF_BRIDGE: IFLA_BRIDGE_TUNNEL_INFO: IFLA_BRIDGE_VLAN_TUNNEL_FLAGS */
	static const uint16_t bti_flags[] = {
		0,
		BRIDGE_VLAN_INFO_MASTER,
		BRIDGE_VLAN_INFO_PVID,
		BRIDGE_VLAN_INFO_MASTER | BRIDGE_VLAN_INFO_PVID,
		0xef80,
		0xcafe,
	};
	for (size_t i = 0; i < ARRAY_SIZE(bti_flags); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
				init_IFLA_BRIDGE_VLAN_TUNNEL_INFO_msg,
				print_IFLA_BRIDGE_VLAN_TUNNEL_INFO_msg,
				IFLA_BRIDGE_VLAN_TUNNEL_FLAGS,
				"IFLA_BRIDGE_VLAN_TUNNEL_FLAGS", NULL,
				pattern, bti_flags[i], jsonl_nl_bytes, 2,
				jsonl_flags(bti_flags[i], bridge_vlan_flags,
					    ARRAY_SIZE(bridge_vlan_flags),
					    "BRIDGE_VLAN_INFO_???"));
	}

	/* AF_INET6 */
	msg_af = AF_UNIX;
	msg_af_str = "AF_UNIX";

	check_ifla_af_inet6(fd, nlh0, hdrlen,
			    init_AF_INET6_msg, print_AF_INET6_msg, pattern, 2);

	/* AF_MCTP */
	JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
				       init_AF_MCTP_msg, print_AF_MCTP_msg,
				       0, "IFLA_MCTP_UNSPEC", NULL, pattern,
				       unknown_msg, jsonl_nl_bytes, 2,
				       jsonl_nl_bytes(unknown_msg,
						      sizeof(unknown_msg)));
	JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
				       init_AF_MCTP_msg, print_AF_MCTP_msg,
				       3, NULL, "IFLA_MCTP_???", pattern,
				       unknown_msg, jsonl_nl_bytes, 2,
				       jsonl_nl_bytes(unknown_msg,
						      sizeof(unknown_msg)));

	/* AF_MCTP: IFLA_MCTP_NET */
	jsonl_check_u32_nlattr(fd, nlh0, hdrlen,
			       init_AF_MCTP_msg, print_AF_MCTP_msg,
			       1, "IFLA_MCTP_NET", NULL, pattern, 2);

	/* AF_MCTP: IFLA_MCTP_PHYS_BINDING */
	jsonl_check_x8_nlattr(fd, nlh0, hdrlen,
			      init_AF_MCTP_msg, print_AF_MCTP_msg,
			      2, "IFLA_MCTP_PHYS_BINDING", NULL, pattern, 2);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
