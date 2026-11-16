/*
 * The JSONL counterpart of nlattr_ifla_af_inet6.h.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef STRACE_TESTS_JSONL_NLATTR_IFLA_AF_INET6_H
# define STRACE_TESTS_JSONL_NLATTR_IFLA_AF_INET6_H

# include "jsonl_netlink.h"

/* An element of the IFLA_INET6_CONF array. */
static inline void
print_inet6_conf_val(const uint32_t *val, size_t idx)
{
	static const char *const syms[] = {
		"DEVCONF_FORWARDING",
		"DEVCONF_HOPLIMIT",
	};

	jsonl_index_const(idx, syms, ARRAY_SIZE(syms), "DEVCONF_???");
	jsonl_decimal((int32_t) *val);
}

/* An element of the IFLA_INET6_STATS array. */
static inline void
print_inet6_stats_val(const uint64_t *val, size_t idx)
{
	static const char *const syms[] = {
		"IPSTATS_MIB_NUM",
		"IPSTATS_MIB_INPKTS",
	};

	jsonl_index_const(idx, syms, ARRAY_SIZE(syms), "IPSTATS_MIB_???");
	jsonl_unsigned(*val);
}

/* An element of the IFLA_INET6_ICMP6STATS array. */
static inline void
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

	jsonl_index_const(idx, syms, ARRAY_SIZE(syms), "ICMP6_MIB_???");
	jsonl_unsigned(*val);
}

/* The JSONL counterpart of check_ifla_af_inet6. */
static inline void
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
				       jsonl_field_unsigned("max_reasm_len",
							    true,
							    ci.max_reasm_len);
				       jsonl_field_unsigned("tstamp", false,
							    ci.tstamp);
				       jsonl_field_unsigned("reachable_time",
							    false,
							    ci.reachable_time);
				       jsonl_field_unsigned("retrans_time",
							    false,
							    ci.retrans_time);
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

#endif /* !STRACE_TESTS_JSONL_NLATTR_IFLA_AF_INET6_H */
