/*
 * Check JSONL output of NHA_* netlink attributes of nhmsg.
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
# include <linux/nexthop.h>

static const unsigned int hdrlen = sizeof(struct nhmsg);

static uint8_t nh_family = AF_UNIX;
static const char *nh_family_str = "AF_UNIX";

/* The attributes that enclose the attribute being checked. */
static struct strval32 nests[2];

static void
init_nhmsg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = RTM_GETNEXTHOP,
		.nlmsg_flags = NLM_F_DUMP,
	);

	struct nhmsg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct nhmsg, msg,
		.nh_family = nh_family,
		.nh_scope = RT_SCOPE_NOWHERE,
		.nh_protocol = RTPROT_UNSPEC,
		.nh_flags = RTNH_F_PERVASIVE | RTNH_F_UNRESOLVED,
	);
}

static void
print_nhmsg(const unsigned int msg_len)
{
	static const struct strval32 nh_flags[] = {
		{ ARG_STR(RTNH_F_PERVASIVE) },
		{ ARG_STR(RTNH_F_UNRESOLVED) },
	};

	jsonl_nlmsghdr_dump_open(msg_len, ARG_STR(RTM_GETNEXTHOP));
	jsonl_struct_open();
	jsonl_field_const("nh_family", true, nh_family, nh_family_str, NULL);
	jsonl_field_const("nh_scope", false, RT_SCOPE_NOWHERE,
			  "RT_SCOPE_NOWHERE", NULL);
	jsonl_field_const("nh_protocol", false, RTPROT_UNSPEC, "RTPROT_UNSPEC",
			  NULL);
	jsonl_field_flags("nh_flags", false,
			  RTNH_F_PERVASIVE | RTNH_F_UNRESOLVED, nh_flags,
			  ARRAY_SIZE(nh_flags), NULL);
	jsonl_struct_close();
	printf("}");
}

static void
init_nhmsg_nested(struct nlmsghdr *const nlh, const unsigned int msg_len,
		  const unsigned int depth)
{
	init_nhmsg(nlh, msg_len);

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
print_nhmsg_nested(const unsigned int msg_len, const unsigned int depth)
{
	print_nhmsg(msg_len);

	for (unsigned int i = 0; i < depth; ++i)
		jsonl_nlattr_nest_open(msg_len - NLMSG_SPACE(hdrlen)
				       - NLA_HDRLEN * i,
				       nests[i].val, nests[i].str, NULL);
}

static void
init_nhmsg_nest1(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	init_nhmsg_nested(nlh, msg_len, 1);
}

static void
print_nhmsg_nest1(const unsigned int msg_len)
{
	print_nhmsg_nested(msg_len, 1);
}

static void
init_nhmsg_nest2(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	init_nhmsg_nested(nlh, msg_len, 2);
}

static void
print_nhmsg_nest2(const unsigned int msg_len)
{
	print_nhmsg_nested(msg_len, 2);
}

static void
print_nh_grp(const struct nexthop_grp *const elem, size_t idx)
{
	jsonl_struct_open();
	jsonl_field_unsigned("id", true, elem->id);
	jsonl_field_unsigned("weight", false, elem->weight);
	jsonl_field_unsigned("weight_high", false, elem->weight_high);
	if (elem->resvd2) {
		jsonl_field_hex("resvd2", false, elem->resvd2);
	}
	jsonl_struct_close();
}

/*
 * An enclosing attribute that is too short for a nested attribute, and
 * unknown and undecoded nested attributes of the enclosing attributes
 * described by nests; unk and pattern describe the unknown attribute types.
 */
static void
check_nest(const int fd, void *const nlh0, const char *const pattern_buf,
	   const unsigned int depth, const unsigned int unk,
	   const char *const pattern, const struct strval32 undecoded)
{
	const unsigned int types[] = { unk, 0xffff & NLA_TYPE_MASK };

	JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
			     depth > 1 ? init_nhmsg_nest1 : init_nhmsg,
			     depth > 1 ? print_nhmsg_nest1 : print_nhmsg,
			     nests[depth - 1].val, nests[depth - 1].str, NULL,
			     3, pattern_buf, 3, depth - 1,
			     jsonl_bytes_value(pattern_buf, 3, false));

	for (size_t i = 0; i < ARRAY_SIZE(types); i++) {
		JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
				     depth > 1 ? init_nhmsg_nest2
					       : init_nhmsg_nest1,
				     depth > 1 ? print_nhmsg_nest2
					       : print_nhmsg_nest1,
				     types[i], NULL, pattern,
				     16, pattern_buf, 16, depth,
				     jsonl_bytes_value(pattern_buf, 16, false));
	}

	JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
			     depth > 1 ? init_nhmsg_nest2 : init_nhmsg_nest1,
			     depth > 1 ? print_nhmsg_nest2 : print_nhmsg_nest1,
			     undecoded.val, undecoded.str, NULL,
			     8, pattern_buf, 8, depth,
			     jsonl_bytes_value(pattern_buf, 8, false));
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen), NLA_HDRLEN + 42);

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	/* Unknown attrs */
	static const uint16_t unk_types[] = { 19, 0xffff & NLA_TYPE_MASK };
	for (size_t i = 0; i < ARRAY_SIZE(unk_types); i++) {
		JSONL_NLATTR_(fd, nlh0, hdrlen, init_nhmsg, print_nhmsg,
			      unk_types[i], NULL, "NHA_???",
			      4, pattern, 4,
			      jsonl_bytes_value(pattern, 4, false));
	}

	/* unimplemented, no semantics: NHA_UNSPEC, NHA_ENCAP */
	static const struct strval32 unimp_types[] = {
		{ ARG_STR(NHA_UNSPEC) },
		{ ARG_STR(NHA_ENCAP) },
	};
	for (size_t i = 0; i < ARRAY_SIZE(unimp_types); i++) {
		JSONL_NLATTR_(fd, nlh0, hdrlen, init_nhmsg, print_nhmsg,
			      unimp_types[i].val, unimp_types[i].str, NULL,
			      42, pattern, 42,
			      jsonl_bytes_value(pattern, DEFAULT_STRLEN, true));
	}

	/* u32 attrs */
	static const struct strval32 u32_attrs[] = {
		{ ARG_STR(NHA_ID) },
		{ ARG_STR(NHA_BLACKHOLE) },
		{ ARG_STR(NHA_GROUPS) },
		{ ARG_STR(NHA_FDB) },
		{ ARG_STR(NHA_HW_STATS_ENABLE) },
		{ ARG_STR(NHA_HW_STATS_USED) },
	};
	for (size_t i = 0; i < ARRAY_SIZE(u32_attrs); i++) {
		jsonl_check_u32_nlattr(fd, nlh0, hdrlen,
				       init_nhmsg, print_nhmsg,
				       u32_attrs[i].val, u32_attrs[i].str, NULL,
				       pattern, 0);
	}

	/* NHA_GROUP */
	static const struct nexthop_grp grps[] = {
		{ .id = 0xdeadbeef },
		{ .weight = 218, .resvd2 = 0xdead },
		{ .id = 0xfacefeed, .weight = 190, .weight_high = 236 },
		{ .weight_high = 202, .resvd2 = 0xbeef },
	};
	JSONL_NLATTR_ARRAY(fd, nlh0, hdrlen, init_nhmsg, print_nhmsg,
			   NHA_GROUP, pattern, grps, print_nh_grp);

	/* NHA_GROUP_TYPE */
	static const struct strval32 grp_types[] = {
		{ ARG_STR(NEXTHOP_GRP_TYPE_MPATH) },
		{ ARG_STR(NEXTHOP_GRP_TYPE_RES) },
		{ 0x2, NULL },
		{ 0xbeef, NULL },
	};
	for (size_t i = 0; i < ARRAY_SIZE(grp_types); i++) {
		const uint16_t val = grp_types[i].val;

		JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_nhmsg, print_nhmsg,
				    NHA_GROUP_TYPE, pattern, val,
				    jsonl_const(val, true, grp_types[i].str,
						"NEXTHOP_GRP_TYPE_???"));
		/* The value is followed by data that cannot be read. */
		JSONL_NLATTR(fd, nlh0, hdrlen, init_nhmsg, print_nhmsg,
			     NHA_GROUP_TYPE, sizeof(val) + 4,
			     &val, sizeof(val),
			     jsonl_const(val, true, grp_types[i].str,
					 "NEXTHOP_GRP_TYPE_???"));
	}

	/* ifindex: NHA_OIF, NHA_MASTER */
	static const struct strval32 if_attrs[] = {
		{ ARG_STR(NHA_OIF) },
		{ ARG_STR(NHA_MASTER) },
	};
	const uint32_t ifindex = ifindex_lo();
	for (size_t i = 0; i < ARRAY_SIZE(if_attrs); i++) {
		static const uint32_t bogus = 0xdeadc0de;

		JSONL_NLATTR_OBJECT_(fd, nlh0, hdrlen, init_nhmsg, print_nhmsg,
				     if_attrs[i].val, if_attrs[i].str, NULL,
				     pattern, bogus,
				     jsonl_ifindex(bogus, NULL));
		JSONL_NLATTR_(fd, nlh0, hdrlen, init_nhmsg, print_nhmsg,
			      if_attrs[i].val, if_attrs[i].str, NULL,
			      sizeof(bogus) + 4, &bogus, sizeof(bogus),
			      jsonl_ifindex(bogus, NULL));

		JSONL_NLATTR_OBJECT_(fd, nlh0, hdrlen, init_nhmsg, print_nhmsg,
				     if_attrs[i].val, if_attrs[i].str, NULL,
				     pattern, ifindex,
				     jsonl_ifindex(ifindex, "lo"));
		JSONL_NLATTR_(fd, nlh0, hdrlen, init_nhmsg, print_nhmsg,
			      if_attrs[i].val, if_attrs[i].str, NULL,
			      sizeof(ifindex) + 4, &ifindex, sizeof(ifindex),
			      jsonl_ifindex(ifindex, "lo"));
	}

	/* NHA_GATEWAY */
	static const struct {
		uint8_t af;
		const char *af_str;
		uint8_t addr[16];
		const char *str;
		uint32_t len;
	} addrs[] = {
		{ ARG_STR(AF_UNIX), { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 },
		  NULL, 10 },
		{ ARG_STR(AF_INET), { 0xde, 0xca, 0xff, 0xed },
		  "222.202.255.237", 4 },
		{ ARG_STR(AF_INET6), { 0xfa, 0xce, 0xbe, 0xef, [15] = 0xda },
		  "face:beef::da", 16 },
	};
	for (size_t i = 0; i < ARRAY_SIZE(addrs); i++) {
		const uint32_t len = addrs[i].len;

		nh_family = addrs[i].af;
		nh_family_str = addrs[i].af_str;
		JSONL_NLATTR(fd, nlh0, hdrlen, init_nhmsg, print_nhmsg,
			     NHA_GATEWAY, len - 1, addrs[i].addr, len - 1,
			     jsonl_bytes_value(addrs[i].addr, len - 1, false));
		JSONL_NLATTR(fd, nlh0, hdrlen, init_nhmsg, print_nhmsg,
			     NHA_GATEWAY, len, addrs[i].addr, len,
			     if (addrs[i].str)
				jsonl_ip_addr(addrs[i].af, addrs[i].af_str,
					      addrs[i].str);
			     else
				jsonl_bytes_value(addrs[i].addr, len, false));
	}
	nh_family = AF_UNIX;
	nh_family_str = "AF_UNIX";

	/* NHA_ENCAP_TYPE */
	static const struct strval32 enc_types[] = {
		{ 0, "LWTUNNEL_ENCAP_NONE" },
		{ 0x8, "LWTUNNEL_ENCAP_RPL" },
		{ 0x9, NULL },
		{ 0xbeef, NULL },
	};
	for (size_t i = 0; i < ARRAY_SIZE(enc_types); i++) {
		const uint16_t val = enc_types[i].val;

		JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_nhmsg, print_nhmsg,
				    NHA_ENCAP_TYPE, pattern, val,
				    jsonl_const(val, true, enc_types[i].str,
						"LWTUNNEL_ENCAP_???"));
		JSONL_NLATTR(fd, nlh0, hdrlen, init_nhmsg, print_nhmsg,
			     NHA_ENCAP_TYPE, sizeof(val) + 4,
			     &val, sizeof(val),
			     jsonl_const(val, true, enc_types[i].str,
					 "LWTUNNEL_ENCAP_???"));
	}

	/* NHA_RES_GROUP */
	nests[0] = (struct strval32) { ARG_STR(NHA_RES_GROUP) };
	check_nest(fd, nlh0, pattern, 1, 5, "NHA_RES_GROUP_???",
		   (struct strval32) { ARG_STR(NHA_RES_GROUP_PAD) });

	JSONL_NLATTR_VAL(u16, fd, nlh0, hdrlen,
			 init_nhmsg_nest1, print_nhmsg_nest1,
			 NHA_RES_GROUP_BUCKETS, pattern, 1);

	static const struct strval32 res_grp_clk_attrs[] = {
		{ ARG_STR(NHA_RES_GROUP_IDLE_TIMER) },
		{ ARG_STR(NHA_RES_GROUP_UNBALANCED_TIMER) },
		{ ARG_STR(NHA_RES_GROUP_UNBALANCED_TIME) },
	};
	for (size_t i = 0; i < ARRAY_SIZE(res_grp_clk_attrs); i++) {
		jsonl_check_clock_t_nlattr(fd, nlh0, hdrlen,
					   init_nhmsg_nest1, print_nhmsg_nest1,
					   res_grp_clk_attrs[i].val,
					   res_grp_clk_attrs[i].str, NULL, 1);
	}

	/* NHA_RES_BUCKET */
	nests[0] = (struct strval32) { ARG_STR(NHA_RES_BUCKET) };
	check_nest(fd, nlh0, pattern, 1, 4, "NHA_RES_BUCKET_???",
		   (struct strval32) { ARG_STR(NHA_RES_BUCKET_PAD) });

	JSONL_NLATTR_VAL(u16, fd, nlh0, hdrlen,
			 init_nhmsg_nest1, print_nhmsg_nest1,
			 NHA_RES_BUCKET_INDEX, pattern, 1);

	jsonl_check_clock_t_nlattr(fd, nlh0, hdrlen,
				   init_nhmsg_nest1, print_nhmsg_nest1,
				   NHA_RES_BUCKET_IDLE_TIME,
				   "NHA_RES_BUCKET_IDLE_TIME", NULL, 1);

	JSONL_NLATTR_VAL(u32, fd, nlh0, hdrlen,
			 init_nhmsg_nest1, print_nhmsg_nest1,
			 NHA_RES_BUCKET_NH_ID, pattern, 1);

	/* NHA_OP_FLAGS */
	static const struct strval32 nha_op_flags[] = {
		{ 0x1, "NHA_OP_FLAG_DUMP_STATS" },
		{ 0x2, "NHA_OP_FLAG_DUMP_HW_STATS" },
		{ 0x80000000, "NHA_OP_FLAG_RESP_GRP_RESVD_0" },
	};
	static const uint32_t op_flags[] = {
		0, 0x1, 0x2, 0x3, 0x80000000, 0x80000003, 0xdeadbeef
	};
	for (size_t i = 0; i < ARRAY_SIZE(op_flags); i++) {
		JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_nhmsg, print_nhmsg,
				    NHA_OP_FLAGS, pattern, op_flags[i],
				    jsonl_flags(op_flags[i], nha_op_flags,
						ARRAY_SIZE(nha_op_flags),
						"NHA_OP_FLAG_???"));
	}

	/* NHA_GROUP_STATS */
	nests[0] = (struct strval32) { ARG_STR(NHA_GROUP_STATS) };
	check_nest(fd, nlh0, pattern, 1, 2, "NHA_GROUP_STATS_???",
		   (struct strval32) { ARG_STR(NHA_GROUP_STATS_UNSPEC) });

	/* NHA_GROUP_STATS_ENTRY */
	nests[1] = (struct strval32) { ARG_STR(NHA_GROUP_STATS_ENTRY) };
	check_nest(fd, nlh0, pattern, 2, 4, "NHA_GROUP_STATS_ENTRY_???",
		   (struct strval32) {
			ARG_STR(NHA_GROUP_STATS_ENTRY_UNSPEC) });

	JSONL_NLATTR_VAL(u32, fd, nlh0, hdrlen,
			 init_nhmsg_nest2, print_nhmsg_nest2,
			 NHA_GROUP_STATS_ENTRY_ID, pattern, 2);

	JSONL_NLATTR_VAL(u64, fd, nlh0, hdrlen,
			 init_nhmsg_nest2, print_nhmsg_nest2,
			 NHA_GROUP_STATS_ENTRY_PACKETS, pattern, 2);
	JSONL_NLATTR_VAL(u64, fd, nlh0, hdrlen,
			 init_nhmsg_nest2, print_nhmsg_nest2,
			 NHA_GROUP_STATS_ENTRY_PACKETS_HW, pattern, 2);

	/* be16: NHA_DST_PORT */
	static const uint16_t ports[] = { 0, 0x1234, 0x8000, 0xabcd, 0xffff };
	for (size_t i = 0; i < ARRAY_SIZE(ports); i++) {
		JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_nhmsg, print_nhmsg,
				    NHA_DST_PORT, pattern, ports[i],
				    jsonl_unsigned(ntohs(ports[i]));
				    jsonl_byteorder_big(sizeof(ports[i])));
	}

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
