/*
 * Check JSONL output of IFLA_STATS_* netlink attributes of if_stats_msg.
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
# include <linux/if_link.h>
# include <linux/if_bonding.h>
# include <linux/if_bridge.h>
# include <linux/mpls.h>

# ifndef AF_MPLS
#  define AF_MPLS 28
# endif

static const unsigned int hdrlen = sizeof(struct if_stats_msg);
static char pattern[4096];

/* The attributes that enclose the attribute being checked. */
static struct strval32 nests[3];

static void
init_ifstats(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = RTM_GETSTATS,
		.nlmsg_flags = NLM_F_DUMP,
	);

	struct if_stats_msg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct if_stats_msg, msg,
		.family = AF_UNIX,
		.ifindex = ifindex_lo(),
		.filter_mask = 0x22,
	);
}

static void
print_ifstats(const unsigned int msg_len)
{
	static const struct strval32 filter_mask[] = {
		{ 1 << IFLA_STATS_LINK_64, "1<<IFLA_STATS_LINK_64" },
		{ 1 << IFLA_STATS_AF_SPEC, "1<<IFLA_STATS_AF_SPEC" },
	};

	jsonl_nlmsghdr_dump_open(msg_len, ARG_STR(RTM_GETSTATS));
	jsonl_struct_open();
	jsonl_field_const("family", true, AF_UNIX, "AF_UNIX", NULL);
	jsonl_field_ifindex("ifindex", false, ifindex_lo(), "lo");
	jsonl_field_flags("filter_mask", false, 0x22, filter_mask,
			  ARRAY_SIZE(filter_mask), NULL);
	jsonl_struct_close();
	printf("}");
}

static void
init_ifstats_nested(struct nlmsghdr *const nlh, const unsigned int msg_len,
		    const unsigned int depth)
{
	init_ifstats(nlh, msg_len);

	for (unsigned int i = 0; i < depth; ++i) {
		struct nlattr *nla = NLMSG_ATTR(nlh, hdrlen + NLA_HDRLEN * i);
		SET_STRUCT(struct nlattr, nla,
			.nla_len = msg_len - NLMSG_SPACE(hdrlen)
				   - NLA_HDRLEN * i,
			.nla_type = nests[i].val,
		);
	}
}

static void
print_ifstats_nested(const unsigned int msg_len, const unsigned int depth)
{
	print_ifstats(msg_len);

	for (unsigned int i = 0; i < depth; ++i)
		jsonl_nlattr_nest_open(msg_len - NLMSG_SPACE(hdrlen)
				       - NLA_HDRLEN * i,
				       nests[i].val, nests[i].str, NULL);
}

# define DEF_NLATTR_FUNCS_NESTED(depth_)				\
	static void							\
	init_ifstats_l##depth_(struct nlmsghdr *const nlh,		\
			       const unsigned int msg_len)		\
	{								\
		init_ifstats_nested(nlh, msg_len, (depth_));		\
	}								\
									\
	static void							\
	print_ifstats_l##depth_(const unsigned int msg_len)		\
	{								\
		print_ifstats_nested(msg_len, (depth_));		\
	}								\
	/* End of DEF_NLATTR_FUNCS_NESTED */

DEF_NLATTR_FUNCS_NESTED(1)
DEF_NLATTR_FUNCS_NESTED(2)
DEF_NLATTR_FUNCS_NESTED(3)

typedef void (*init_fn)(struct nlmsghdr *, unsigned int);
typedef void (*print_fn)(unsigned int);

static const init_fn init_fns[] = {
	init_ifstats, init_ifstats_l1, init_ifstats_l2, init_ifstats_l3
};
static const print_fn print_fns[] = {
	print_ifstats, print_ifstats_l1, print_ifstats_l2, print_ifstats_l3
};

/* The separator between two elements of an array of values. */
static void
next_elem(void)
{
	printf("}, {");
}

/* The bytes that follow a decoded structure, as a separate element. */
static void
print_trailing_bytes(const void *p, size_t len)
{
	next_elem();
	jsonl_bytes_value(p, MIN(len, DEFAULT_STRLEN), len > DEFAULT_STRLEN);
}

/* The first size bytes of a struct rtnl_link_stats64. */
static void
print_stats_64(const struct rtnl_link_stats64 *const st, const size_t size)
{
	static const char *const names[] = {
		"rx_packets", "tx_packets", "rx_bytes", "tx_bytes",
		"rx_errors", "tx_errors", "rx_dropped", "tx_dropped",
		"multicast", "collisions", "rx_length_errors",
		"rx_over_errors", "rx_crc_errors", "rx_frame_errors",
		"rx_fifo_errors", "rx_missed_errors", "tx_aborted_errors",
		"tx_carrier_errors", "tx_fifo_errors", "tx_heartbeat_errors",
		"tx_window_errors", "rx_compressed", "tx_compressed",
		"rx_nohandler", "rx_otherhost_dropped"
	};
	uint64_t vals[sizeof(*st) / sizeof(uint64_t)];

	memcpy(vals, st, sizeof(vals));
	jsonl_struct_open();
	for (size_t i = 0; i < size / sizeof(vals[0]); ++i)
		jsonl_field_unsigned(names[i], i == 0, vals[i]);
	jsonl_struct_close();
}

/*
 * A struct rtnl_link_stats64 attribute at the nesting level depth:
 * the whole structure and the structure without its newest fields.
 */
static void
check_stats_64(const int fd, void *const nlh0, const unsigned int cmd,
	       const char *const cmd_str, const unsigned int depth)
{
	static const size_t minsz = offsetofend(struct rtnl_link_stats64,
						tx_compressed);
	struct rtnl_link_stats64 st;

	fill_memory(&st, sizeof(st));

	JSONL_NESTED_NLATTR_OBJPTR_EX_(fd, nlh0, hdrlen,
				       init_fns[depth], print_fns[depth],
				       cmd, cmd_str, NULL,
				       pattern, &st, sizeof(st), minsz,
				       jsonl_nl_bytes, depth,
				       print_stats_64(&st, sizeof(st)));

	JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
			     init_fns[depth], print_fns[depth],
			     cmd, cmd_str, NULL,
			     minsz, &st, minsz, depth,
			     print_stats_64(&st, minsz));
}

/*
 * Attributes at the nesting level depth that are not decoded: the first
 * one is known, the others are unknown and described by sym_pattern.
 */
static void
check_undecoded(const int fd, void *const nlh0, const unsigned int depth,
		const struct strval32 known, const unsigned int unknown,
		const char *const sym_pattern)
{
	const struct strval32 types[] = {
		known,
		{ unknown, NULL },
		{ 0xbad, NULL },
	};

	for (size_t i = 0; i < ARRAY_SIZE(types); i++) {
		JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
				     init_fns[depth], print_fns[depth],
				     types[i].val, types[i].str, sym_pattern,
				     37, pattern, 37, depth,
				     jsonl_bytes_value(pattern, DEFAULT_STRLEN,
						       true));
	}
}

static void
print_vlan_xstats(const struct bridge_vlan_xstats *const st)
{
	static const struct strval32 vlan_flags[] = {
		{ ARG_STR(BRIDGE_VLAN_INFO_MASTER) },
		{ ARG_STR(BRIDGE_VLAN_INFO_PVID) },
		{ ARG_STR(BRIDGE_VLAN_INFO_UNTAGGED) },
		{ ARG_STR(BRIDGE_VLAN_INFO_RANGE_BEGIN) },
		{ ARG_STR(BRIDGE_VLAN_INFO_RANGE_END) },
		{ ARG_STR(BRIDGE_VLAN_INFO_BRENTRY) },
		{ ARG_STR(BRIDGE_VLAN_INFO_ONLY_OPTS) },
	};

	jsonl_struct_open();
	jsonl_field_unsigned("rx_bytes", true, st->rx_bytes);
	jsonl_field_unsigned("rx_packets", false, st->rx_packets);
	jsonl_field_unsigned("tx_bytes", false, st->tx_bytes);
	jsonl_field_unsigned("tx_packets", false, st->tx_packets);
	jsonl_field_unsigned("vid", false, st->vid);
	jsonl_field_flags("flags", false, st->flags, vlan_flags,
			  ARRAY_SIZE(vlan_flags), "BRIDGE_VLAN_INFO_???");
	if (st->pad2) {
		jsonl_field_hex("pad2", false, st->pad2);
	}
	jsonl_struct_close();
}

/* A field with a counter for each of the two multicast directions. */
static void
print_mcast_dir_field(const char *name, const uint64_t *const vals, bool first)
{
	static const char *const dirs[] = {
		"BR_MCAST_DIR_RX", "BR_MCAST_DIR_TX"
	};

	jsonl_field_open(name, first);
	jsonl_array_open();
	for (size_t i = 0; i < ARRAY_SIZE(dirs); ++i) {
		printf("%s{\"index\": {", i ? ", " : "");
		jsonl_const(i, false, dirs[i], NULL);
		printf("}, ");
		jsonl_unsigned(vals[i]);
		printf("}");
	}
	jsonl_array_close();
	jsonl_field_close();
}

static void
print_mcast_stats(const struct br_mcast_stats *const st)
{
# define PR_DIR_FIELD_(field_, first_)					\
	do {								\
		const uint64_t vals_[] = {				\
			st->field_[0], st->field_[1]			\
		};							\
		print_mcast_dir_field(#field_, vals_, (first_));	\
	} while (0)

	jsonl_struct_open();
	PR_DIR_FIELD_(igmp_v1queries, true);
	PR_DIR_FIELD_(igmp_v2queries, false);
	PR_DIR_FIELD_(igmp_v3queries, false);
	PR_DIR_FIELD_(igmp_leaves, false);
	PR_DIR_FIELD_(igmp_v1reports, false);
	PR_DIR_FIELD_(igmp_v2reports, false);
	PR_DIR_FIELD_(igmp_v3reports, false);
	jsonl_field_unsigned("igmp_parse_errors", false, st->igmp_parse_errors);
	PR_DIR_FIELD_(mld_v1queries, false);
	PR_DIR_FIELD_(mld_v2queries, false);
	PR_DIR_FIELD_(mld_leaves, false);
	PR_DIR_FIELD_(mld_v1reports, false);
	PR_DIR_FIELD_(mld_v2reports, false);
	jsonl_field_unsigned("mld_parse_errors", false, st->mld_parse_errors);
	PR_DIR_FIELD_(mcast_bytes, false);
	PR_DIR_FIELD_(mcast_packets, false);
	jsonl_struct_close();

# undef PR_DIR_FIELD_
}

static void
print_stp_xstats(const struct bridge_stp_xstats *const st)
{
	jsonl_struct_open();
	jsonl_field_unsigned("transition_blk", true, st->transition_blk);
	jsonl_field_unsigned("transition_fwd", false, st->transition_fwd);
	jsonl_field_unsigned("rx_bpdu", false, st->rx_bpdu);
	jsonl_field_unsigned("tx_bpdu", false, st->tx_bpdu);
	jsonl_field_unsigned("rx_tcn", false, st->rx_tcn);
	jsonl_field_unsigned("tx_tcn", false, st->tx_tcn);
	jsonl_struct_close();
}

static void
check_xstats(const int fd, void *const nlh0, const unsigned int cmd,
	     const char *const cmd_str)
{
	nests[0] = (struct strval32) { cmd, cmd_str };

	check_undecoded(fd, nlh0, 1,
			(struct strval32) { ARG_STR(LINK_XSTATS_TYPE_UNSPEC) },
			0x3, "LINK_XSTATS_TYPE_???");

	/* LINK_XSTATS_TYPE_BRIDGE */
	nests[1] = (struct strval32) { ARG_STR(LINK_XSTATS_TYPE_BRIDGE) };

	check_undecoded(fd, nlh0, 2,
			(struct strval32) { ARG_STR(BRIDGE_XSTATS_UNSPEC) },
			0x5, "BRIDGE_XSTATS_???");
	JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
			     init_ifstats_l2, print_ifstats_l2,
			     BRIDGE_XSTATS_PAD, "BRIDGE_XSTATS_PAD", NULL,
			     37, pattern, 37, 2,
			     jsonl_bytes_value(pattern, DEFAULT_STRLEN, true));

	/* LINK_XSTATS_TYPE_BRIDGE: BRIDGE_XSTATS_VLAN */
	static const struct bridge_vlan_xstats br_xst_vlan_vecs[] = {
		{ .rx_packets = 0xdeadfacebeeffeedULL,
		  .tx_bytes = 0x8090a0b0c0d0e0f0ULL,
		  .vid = 0xdead, .flags = 2 },
		{ .rx_bytes = 12345678901234567890ULL,
		  .tx_packets = 9876543210987654321ULL,
		  .pad2 = 0xbadc0ded },
		{ .flags = 0xdeed },
		{ .rx_bytes = 0xdefaceddecaffeedULL,
		  .rx_packets = 0xbeeffacedeadbabeULL,
		  .tx_bytes = 0xbeeffeeddadfacedULL,
		  .tx_packets = 0xbeeffadeeffaceedULL,
		  .vid = 0xcafe, .flags = 0xfa80, .pad2 = 0xdeadabba },
	};

	for (size_t i = 0; i < ARRAY_SIZE(br_xst_vlan_vecs); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
			init_ifstats_l2, print_ifstats_l2,
			BRIDGE_XSTATS_VLAN, pattern, br_xst_vlan_vecs[i], 2,
			print_vlan_xstats(&br_xst_vlan_vecs[i]));

		char buf[sizeof(br_xst_vlan_vecs[0]) + 42];
		fill_memory(buf, sizeof(buf));
		memcpy(buf, &br_xst_vlan_vecs[i], sizeof(br_xst_vlan_vecs[i]));
		JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
			init_ifstats_l2, print_ifstats_l2,
			BRIDGE_XSTATS_VLAN, "BRIDGE_XSTATS_VLAN", NULL,
			sizeof(buf), buf, sizeof(buf), 2,
			print_vlan_xstats(&br_xst_vlan_vecs[i]);
			print_trailing_bytes(buf + sizeof(br_xst_vlan_vecs[0]),
					     42));
	}

	/* LINK_XSTATS_TYPE_BRIDGE: BRIDGE_XSTATS_MCAST */
	struct br_mcast_stats br_xst_mc;

	memset(&br_xst_mc, 0, sizeof(br_xst_mc));
	JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
				      init_ifstats_l2, print_ifstats_l2,
				      BRIDGE_XSTATS_MCAST, pattern, br_xst_mc,
				      2, print_mcast_stats(&br_xst_mc));

	fill_memory64(&br_xst_mc, sizeof(br_xst_mc));
	JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
				      init_ifstats_l2, print_ifstats_l2,
				      BRIDGE_XSTATS_MCAST, pattern, br_xst_mc,
				      2, print_mcast_stats(&br_xst_mc));

	char mc_buf[sizeof(br_xst_mc) + 8];
	fill_memory(mc_buf, sizeof(mc_buf));
	memcpy(mc_buf, &br_xst_mc, sizeof(br_xst_mc));
	JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
			     init_ifstats_l2, print_ifstats_l2,
			     BRIDGE_XSTATS_MCAST, "BRIDGE_XSTATS_MCAST", NULL,
			     sizeof(mc_buf), mc_buf, sizeof(mc_buf), 2,
			     print_mcast_stats(&br_xst_mc);
			     print_trailing_bytes(mc_buf + sizeof(br_xst_mc),
						  8));

	/* LINK_XSTATS_TYPE_BRIDGE: BRIDGE_XSTATS_STP */
	static const struct bridge_stp_xstats br_xst_stp_vecs[] = {
		{ .transition_blk = 0 },
		{ .transition_blk = 0x8090a0b0c0d0e0f0ULL,
		  .transition_fwd = 0x8191a1b1c1d1e1f1ULL,
		  .rx_bpdu = 0x8292a2b2c2d2e2f2ULL,
		  .tx_bpdu = 0x8393a3b3c3d3e3f3ULL,
		  .rx_tcn = 0x8494a4b4c4d4e4f4ULL,
		  .tx_tcn = 0x8595a5b5c5d5e5f5ULL, },
	};

	for (size_t i = 0; i < ARRAY_SIZE(br_xst_stp_vecs); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
			init_ifstats_l2, print_ifstats_l2,
			BRIDGE_XSTATS_STP, pattern, br_xst_stp_vecs[i], 2,
			print_stp_xstats(&br_xst_stp_vecs[i]));

		char buf[sizeof(br_xst_stp_vecs[0]) + 33];
		fill_memory(buf, sizeof(buf));
		memcpy(buf, &br_xst_stp_vecs[i], sizeof(br_xst_stp_vecs[i]));
		JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
			init_ifstats_l2, print_ifstats_l2,
			BRIDGE_XSTATS_STP, "BRIDGE_XSTATS_STP", NULL,
			sizeof(buf), buf, sizeof(buf), 2,
			print_stp_xstats(&br_xst_stp_vecs[i]);
			print_trailing_bytes(buf + sizeof(br_xst_stp_vecs[0]),
					     33));
	}

	/* LINK_XSTATS_TYPE_BOND */
	nests[1] = (struct strval32) { ARG_STR(LINK_XSTATS_TYPE_BOND) };

	check_undecoded(fd, nlh0, 2,
			(struct strval32) { ARG_STR(BOND_XSTATS_UNSPEC) },
			0x2, "BOND_XSTATS_???");

	/* LINK_XSTATS_TYPE_BOND: BOND_XSTATS_3AD */
	nests[2] = (struct strval32) { ARG_STR(BOND_XSTATS_3AD) };

	check_undecoded(fd, nlh0, 3,
			(struct strval32) { ARG_STR(BOND_3AD_STAT_PAD) },
			0xa, "BOND_3AD_STAT_???");

	static const struct strval32 u64_3ad_types[] = {
		{ ARG_STR(BOND_3AD_STAT_LACPDU_RX) },
		{ ARG_STR(BOND_3AD_STAT_LACPDU_TX) },
		{ ARG_STR(BOND_3AD_STAT_LACPDU_UNKNOWN_RX) },
		{ ARG_STR(BOND_3AD_STAT_LACPDU_ILLEGAL_RX) },
		{ ARG_STR(BOND_3AD_STAT_MARKER_RX) },
		{ ARG_STR(BOND_3AD_STAT_MARKER_TX) },
		{ ARG_STR(BOND_3AD_STAT_MARKER_RESP_RX) },
		{ ARG_STR(BOND_3AD_STAT_MARKER_RESP_TX) },
		{ ARG_STR(BOND_3AD_STAT_MARKER_UNKNOWN_RX) },
	};
	for (size_t i = 0; i < ARRAY_SIZE(u64_3ad_types); i++) {
		jsonl_check_u64_nlattr(fd, nlh0, hdrlen,
				       init_ifstats_l3, print_ifstats_l3,
				       u64_3ad_types[i].val,
				       u64_3ad_types[i].str, NULL,
				       pattern, 3);
	}
}

static void
check_stats_offload(const int fd, void *const nlh0)
{
	nests[0] = (struct strval32) {
		ARG_STR(IFLA_STATS_LINK_OFFLOAD_XSTATS)
	};

	check_undecoded(fd, nlh0, 1,
			(struct strval32) {
				ARG_STR(IFLA_OFFLOAD_XSTATS_UNSPEC) },
			0x2, "IFLA_OFFLOAD_XSTATS_???");

	check_stats_64(fd, nlh0, ARG_STR(IFLA_OFFLOAD_XSTATS_CPU_HIT), 1);
}

/* Address families that have no decoder of their IFLA_STATS_AF_SPEC. */
static void
check_stats_af_generic(const int fd, void *const nlh0)
{
	enum { ATTR_SZ = NLA_HDRLEN + 2 * NLA_HDRLEN + 4 + 8 };

	struct {
		struct nlattr hdr;

		struct nlattr nested_hdr1;
		uint32_t nested_data1;

		struct nlattr nested_hdr2;
		uint64_t nested_data2;
	} dummy_data = {
		{ ATTR_SZ, 0 /* AF_* */ },
		{ NLA_HDRLEN + sizeof(uint32_t), 0 }, 0xdeadc0de,
		{ NLA_HDRLEN + sizeof(uint64_t), 1 }, 0xbadda7adeadfacedULL,
	};
	static const struct strval32 afs[] = {
		{ ARG_STR(AF_UNSPEC) },
		{ ARG_STR(AF_UNIX) },
		{ ARG_STR(AF_INET) },
		{ ARG_STR(AF_INET6) },
		{ 0xff, NULL },
	};

	static_assert(ATTR_SZ == sizeof(dummy_data),
		      "Dummy nlattr payload size mismatch");

	for (size_t i = 0; i < ARRAY_SIZE(afs); i++) {
		dummy_data.hdr.nla_type = afs[i].val;
		JSONL_NLATTR(fd, nlh0, hdrlen, init_ifstats, print_ifstats,
			     IFLA_STATS_AF_SPEC,
			     ATTR_SZ, &dummy_data, ATTR_SZ,
			     jsonl_nlattr_open(ATTR_SZ, afs[i].val, afs[i].str,
					       "AF_???");
			     jsonl_bytes_value(&dummy_data.nested_hdr1,
					       sizeof(dummy_data) - NLA_HDRLEN,
					       false);
			     jsonl_nlattr_close());
	}
}

static void
print_mpls_link_stats(const struct mpls_link_stats *const mls)
{
	jsonl_struct_open();
	jsonl_field_unsigned("rx_packets", true, mls->rx_packets);
	jsonl_field_unsigned("tx_packets", false, mls->tx_packets);
	jsonl_field_unsigned("rx_bytes", false, mls->rx_bytes);
	jsonl_field_unsigned("tx_bytes", false, mls->tx_bytes);
	jsonl_field_unsigned("rx_errors", false, mls->rx_errors);
	jsonl_field_unsigned("tx_errors", false, mls->tx_errors);
	jsonl_field_unsigned("rx_dropped", false, mls->rx_dropped);
	jsonl_field_unsigned("tx_dropped", false, mls->tx_dropped);
	jsonl_field_unsigned("rx_noroute", false, mls->rx_noroute);
	jsonl_struct_close();
}

static void
check_stats_af_mpls(const int fd, void *const nlh0)
{
	nests[0] = (struct strval32) { ARG_STR(IFLA_STATS_AF_SPEC) };
	nests[1] = (struct strval32) { ARG_STR(AF_MPLS) };

	check_undecoded(fd, nlh0, 2,
			(struct strval32) { ARG_STR(MPLS_STATS_UNSPEC) },
			0x2, "MPLS_STATS_???");

	/* MPLS_STATS_LINK */
	struct mpls_link_stats mls;

	memset(&mls, 0, sizeof(mls));
	JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
				      init_ifstats_l2, print_ifstats_l2,
				      MPLS_STATS_LINK, pattern, mls, 2,
				      print_mpls_link_stats(&mls));

	fill_memory64(&mls, sizeof(mls));
	JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
				      init_ifstats_l2, print_ifstats_l2,
				      MPLS_STATS_LINK, pattern, mls, 2,
				      print_mpls_link_stats(&mls));

	char mls_buf[sizeof(mls) + 32];
	fill_memory(mls_buf, sizeof(mls_buf));
	memcpy(mls_buf, &mls, sizeof(mls));
	JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
			     init_ifstats_l2, print_ifstats_l2,
			     MPLS_STATS_LINK, "MPLS_STATS_LINK", NULL,
			     sizeof(mls_buf), mls_buf, sizeof(mls_buf), 2,
			     print_mpls_link_stats(&mls);
			     print_trailing_bytes(mls_buf + sizeof(mls), 32));
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen), 4 * NLA_HDRLEN + 256);

	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	/* Unknown attrs */
	static const uint16_t unk_types[] = { 6, 0xffff & NLA_TYPE_MASK };
	for (size_t i = 0; i < ARRAY_SIZE(unk_types); i++) {
		JSONL_NLATTR_(fd, nlh0, hdrlen, init_ifstats, print_ifstats,
			      unk_types[i], NULL, "IFLA_STATS_???",
			      4, pattern, 4,
			      jsonl_bytes_value(pattern, 4, false));
	}

	/* IFLA_STATS_UNSPEC: unimplemented, no semantics */
	JSONL_NLATTR(fd, nlh0, hdrlen, init_ifstats, print_ifstats,
		     IFLA_STATS_UNSPEC, 42, pattern, 42,
		     jsonl_bytes_value(pattern, DEFAULT_STRLEN, true));

	/* IFLA_STATS_LINK_64 */
	check_stats_64(fd, nlh0, ARG_STR(IFLA_STATS_LINK_64), 0);

	/* IFLA_STATS_LINK_XSTATS, IFLA_STATS_LINK_XSTATS_SLAVE */
	check_xstats(fd, nlh0, ARG_STR(IFLA_STATS_LINK_XSTATS));
	check_xstats(fd, nlh0, ARG_STR(IFLA_STATS_LINK_XSTATS_SLAVE));

	/* IFLA_STATS_LINK_OFFLOAD_XSTATS */
	check_stats_offload(fd, nlh0);

	/* IFLA_STATS_AF_SPEC */
	check_stats_af_generic(fd, nlh0);
	check_stats_af_mpls(fd, nlh0);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
