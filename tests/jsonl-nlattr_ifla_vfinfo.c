/*
 * Check JSONL output of IFLA_VFINFO_LIST netlink attributes.
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

# include <linux/if.h>
# include <linux/if_arp.h>
# include <linux/if_ether.h>
# include <linux/if_link.h>

# define IFLA_ATTR IFLA_VFINFO_LIST

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

# define VF_NEST_FUNCS(name_, parent_, nla_type_, depth_)		\
	static void							\
	init_##name_##_msg(struct nlmsghdr *const nlh,			\
			   const unsigned int msg_len)			\
	{								\
		init_##parent_(nlh, msg_len);				\
									\
		struct nlattr *nla = NLMSG_ATTR(nlh, hdrlen);		\
		nla += (depth_);					\
		SET_STRUCT(struct nlattr, nla,				\
			.nla_len = msg_len - NLMSG_SPACE(hdrlen)	\
				  - NLA_HDRLEN * (depth_),		\
			.nla_type = nla_type_,				\
		);							\
	}								\
									\
	static void							\
	print_##name_##_msg(const unsigned int msg_len)			\
	{								\
		print_##parent_(msg_len);				\
		jsonl_nlattr_nest_open(msg_len - NLMSG_SPACE(hdrlen)	\
				       - NLA_HDRLEN * (depth_),		\
				       nla_type_, #nla_type_, NULL);	\
	}								\
	/* end of VF_NEST_FUNCS definition */

VF_NEST_FUNCS(vf_info, ifinfomsg, IFLA_VF_INFO, 1)
VF_NEST_FUNCS(vf_stats, vf_info_msg, IFLA_VF_STATS, 2)
VF_NEST_FUNCS(vlan_list, vf_info_msg, IFLA_VF_VLAN_LIST, 2)

static void
print_u32_field(const char *name, uint32_t val, bool first)
{
	jsonl_field_open(name, first);
	jsonl_unsigned(val);
	jsonl_field_close();
}

/* A struct that consists of the given number of unsigned 32-bit fields. */
static void
print_u32_struct(const uint32_t *vals, const char *const *names, size_t n)
{
	jsonl_struct_open();
	for (size_t i = 0; i < n; i++)
		print_u32_field(names[i], vals[i], i == 0);
	jsonl_struct_close();
}

# define PRINT_U32_STRUCT(obj_, ...)					\
	print_u32_struct((const void *) &(obj_),			\
			 (const char *const []) { __VA_ARGS__ },	\
			 sizeof(obj_) / sizeof(uint32_t))

int
main(int argc, char *argv[])
{
	static const uint8_t dummy[] = { 0xab, 0xac, 0xdb, 0xcd };

	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen), 2 * NLA_HDRLEN + 256);

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	/* unknown IFLA_VF_INFO*, IFLA_VF_INFO_UNSPEC */
	static const struct strval32 unk_attrs[] = {
		{ ARG_STR(IFLA_VF_INFO_UNSPEC) },
		{ 0x2, NULL },
		{ 0x1ace, NULL },
	};
	for (size_t i = 0; i < ARRAY_SIZE(unk_attrs); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
					       init_ifinfomsg, print_ifinfomsg,
					       unk_attrs[i].val,
					       unk_attrs[i].str,
					       "IFLA_VF_INFO_???",
					       pattern, dummy,
					       jsonl_nl_bytes, 1,
					       jsonl_nl_bytes(dummy,
							      sizeof(dummy)));
	}

	/* IFLA_VF_INFO: unknown, IFLA_VF_UNSPEC */
	static const struct strval32 unk_vf_attrs[] = {
		{ ARG_STR(IFLA_VF_UNSPEC) },
		{ 0xe, NULL },
		{ 0x1ace, NULL },
	};
	for (size_t i = 0; i < ARRAY_SIZE(unk_vf_attrs); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
					       init_vf_info_msg,
					       print_vf_info_msg,
					       unk_vf_attrs[i].val,
					       unk_vf_attrs[i].str,
					       "IFLA_VF_???",
					       pattern, dummy,
					       jsonl_nl_bytes, 2,
					       jsonl_nl_bytes(dummy,
							      sizeof(dummy)));
	}

	/* IFLA_VF_INFO: IFLA_VF_MAC */
	struct ifla_vf_mac ifla_vm;
	ifla_vm.vf = 0xdeadface;
	fill_memory(&ifla_vm.mac, sizeof(ifla_vm.mac));
	JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
				      init_vf_info_msg, print_vf_info_msg,
				      IFLA_VF_MAC, pattern, ifla_vm, 2,
				      jsonl_struct_open();
				      print_u32_field("vf", ifla_vm.vf, true);
				      jsonl_field_open("mac", false);
				      jsonl_mac(ifla_vm.mac,
						sizeof(ifla_vm.mac));
				      jsonl_field_close();
				      jsonl_struct_close());

	/* IFLA_VF_INFO: IFLA_VF_VLAN */
	struct ifla_vf_vlan ifla_vv;
	ifla_vv.vf	= 0x80a0c0e0;
	ifla_vv.vlan	= 0x81a1c1e1;
	ifla_vv.qos	= 0x82a2c2e2;
	JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
				      init_vf_info_msg, print_vf_info_msg,
				      IFLA_VF_VLAN, pattern, ifla_vv, 2,
				      PRINT_U32_STRUCT(ifla_vv,
						       "vf", "vlan", "qos"));

	/* IFLA_VF_INFO: IFLA_VF_TX_RATE */
	struct ifla_vf_tx_rate ifla_vtr;
	ifla_vtr.vf	= 0x80a0c0e0;
	ifla_vtr.rate	= 0x81a1c1e1;
	JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
				      init_vf_info_msg, print_vf_info_msg,
				      IFLA_VF_TX_RATE, pattern, ifla_vtr, 2,
				      PRINT_U32_STRUCT(ifla_vtr, "vf", "rate"));

	/* IFLA_VF_INFO: IFLA_VF_SPOOFCHK */
	struct ifla_vf_spoofchk ifla_vsc;
	ifla_vsc.vf		= 0x80a0c0e0;
	ifla_vsc.setting	= 0x81a1c1e1;
	JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
				      init_vf_info_msg, print_vf_info_msg,
				      IFLA_VF_SPOOFCHK, pattern, ifla_vsc, 2,
				      PRINT_U32_STRUCT(ifla_vsc,
						       "vf", "setting"));

	/* IFLA_VF_INFO: IFLA_VF_LINK_STATE */
	static const struct strval32 states[] = {
		{ ARG_STR(IFLA_VF_LINK_STATE_AUTO) },
		{ ARG_STR(IFLA_VF_LINK_STATE_ENABLE) },
		{ ARG_STR(IFLA_VF_LINK_STATE_DISABLE) },
		{ 0x3, NULL },
		{ 0x4, NULL },
		{ 0xfade, NULL },
	};
	struct ifla_vf_link_state ifla_vls;
	ifla_vls.vf = 0xbeeffeed;

	for (size_t i = 0; i < ARRAY_SIZE(states); i++) {
		ifla_vls.link_state = states[i].val;
		JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
				init_vf_info_msg, print_vf_info_msg,
				IFLA_VF_LINK_STATE, pattern, ifla_vls, 2,
				jsonl_struct_open();
				print_u32_field("vf", ifla_vls.vf, true);
				jsonl_field_open("link_state", false);
				jsonl_const(ifla_vls.link_state, true,
					    states[i].str,
					    "IFLA_VF_LINK_STATE_???");
				jsonl_field_close();
				jsonl_struct_close());
	}

	/* IFLA_VF_INFO: IFLA_VF_RATE */
	struct ifla_vf_rate ifla_vr;
	ifla_vr.vf		= 0x80a0c0e0;
	ifla_vr.min_tx_rate	= 0x81a1c1e1;
	ifla_vr.max_tx_rate	= 0x82a2c2e2;
	JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
				      init_vf_info_msg, print_vf_info_msg,
				      IFLA_VF_RATE, pattern, ifla_vr, 2,
				      PRINT_U32_STRUCT(ifla_vr, "vf",
						       "min_tx_rate",
						       "max_tx_rate"));

	/* IFLA_VF_INFO: IFLA_VF_RSS_QUERY_EN */
	struct ifla_vf_rss_query_en ifla_vrqe;
	ifla_vrqe.vf		= 0x80a0c0e0;
	ifla_vrqe.setting	= 0x81a1c1e1;
	JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
				      init_vf_info_msg, print_vf_info_msg,
				      IFLA_VF_RSS_QUERY_EN, pattern, ifla_vrqe,
				      2,
				      PRINT_U32_STRUCT(ifla_vrqe,
						       "vf", "setting"));

	/* IFLA_VF_INFO: IFLA_VF_STATS: unknown, IFLA_VF_STATS_PAD */
	static const struct strval32 unk_vs_attrs[] = {
		{ ARG_STR(IFLA_VF_STATS_PAD) },
		{ 0x9, NULL },
		{ 0x1ace, NULL },
	};
	for (size_t i = 0; i < ARRAY_SIZE(unk_vs_attrs); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
					       init_vf_stats_msg,
					       print_vf_stats_msg,
					       unk_vs_attrs[i].val,
					       unk_vs_attrs[i].str,
					       "IFLA_VF_STATS_???",
					       pattern, dummy,
					       jsonl_nl_bytes, 3,
					       jsonl_nl_bytes(dummy,
							      sizeof(dummy)));
	}

	/* IFLA_VF_INFO: IFLA_VF_STATS: u64 attrs */
	static const struct strval32 u64_vs_attrs[] = {
		{ ARG_STR(IFLA_VF_STATS_RX_PACKETS) },
		{ ARG_STR(IFLA_VF_STATS_TX_PACKETS) },
		{ ARG_STR(IFLA_VF_STATS_RX_BYTES) },
		{ ARG_STR(IFLA_VF_STATS_TX_BYTES) },
		{ ARG_STR(IFLA_VF_STATS_BROADCAST) },
		{ ARG_STR(IFLA_VF_STATS_MULTICAST) },
		{ ARG_STR(IFLA_VF_STATS_RX_DROPPED) },
		{ ARG_STR(IFLA_VF_STATS_TX_DROPPED) },
	};
	void *nlh_vs_u64 = midtail_alloc(NLMSG_SPACE(hdrlen),
					 3 * NLA_HDRLEN + sizeof(uint64_t));

	for (size_t i = 0; i < ARRAY_SIZE(u64_vs_attrs); i++) {
		jsonl_check_u64_nlattr(fd, nlh_vs_u64, hdrlen,
				       init_vf_stats_msg, print_vf_stats_msg,
				       u64_vs_attrs[i].val,
				       u64_vs_attrs[i].str, NULL,
				       pattern, 3);
	}

	/* IFLA_VF_INFO: IFLA_VF_TRUST */
	struct ifla_vf_trust ifla_vt;
	ifla_vt.vf	= 0x80a0c0e0;
	ifla_vt.setting	= 0x81a1c1e1;
	JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
				      init_vf_info_msg, print_vf_info_msg,
				      IFLA_VF_TRUST, pattern, ifla_vt, 2,
				      PRINT_U32_STRUCT(ifla_vt,
						       "vf", "setting"));

	/* IFLA_VF_INFO: IFLA_VF_IB_NODE_GUID, IFLA_VF_IB_PORT_GUID */
	static const struct strval32 guid_attrs[] = {
		{ ARG_STR(IFLA_VF_IB_NODE_GUID) },
		{ ARG_STR(IFLA_VF_IB_PORT_GUID) },
	};
	struct ifla_vf_guid ifla_vg;
	ifla_vg.vf = 0xfacecafe;
	ifla_vg.guid = 0xbadc0deddeedbabeULL;

	for (size_t i = 0; i < ARRAY_SIZE(guid_attrs); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
				init_vf_info_msg, print_vf_info_msg,
				guid_attrs[i].val, guid_attrs[i].str, NULL,
				pattern, ifla_vg, jsonl_nl_bytes, 2,
				jsonl_struct_open();
				print_u32_field("vf", ifla_vg.vf, true);
				jsonl_field_open("guid", false);
				jsonl_hex(ifla_vg.guid);
				jsonl_field_close();
				jsonl_struct_close());
	}

	/* IFLA_VF_INFO: IFLA_VF_VLAN_LIST: unknown, IFLA_VF_VLAN_INFO_UNSPEC */
	static const struct strval32 unk_vl_attrs[] = {
		{ ARG_STR(IFLA_VF_VLAN_INFO_UNSPEC) },
		{ 0x2, NULL },
		{ 0x1ace, NULL },
	};
	for (size_t i = 0; i < ARRAY_SIZE(unk_vl_attrs); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
					       init_vlan_list_msg,
					       print_vlan_list_msg,
					       unk_vl_attrs[i].val,
					       unk_vl_attrs[i].str,
					       "IFLA_VF_VLAN_INFO_???",
					       pattern, dummy,
					       jsonl_nl_bytes, 3,
					       jsonl_nl_bytes(dummy,
							      sizeof(dummy)));
	}

	/* IFLA_VF_INFO: IFLA_VF_VLAN_LIST: IFLA_VF_VLAN_INFO */
	static const struct strval32 eth_protos[] = {
		{ 0, NULL },
		{ ARG_STR(ETH_P_PPP_MP) },
		{ ARG_STR(ETH_P_IP) },
		{ 0xf, NULL },
		{ 0xfb, NULL },
		{ ARG_STR(ETH_P_AF_IUCV) },
		{ 0xffff, NULL },
	};
	struct ifla_vf_vlan_info ifla_vvi;
	ifla_vvi.vf	= 0x80a0c0e0;
	ifla_vvi.vlan	= 0x81a1c1e1;
	ifla_vvi.qos	= 0x82a2c2e2;

	for (size_t i = 0; i < ARRAY_SIZE(eth_protos); i++) {
		ifla_vvi.vlan_proto = htons(eth_protos[i].val);
		JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
				init_vlan_list_msg, print_vlan_list_msg,
				IFLA_VF_VLAN_INFO, pattern, ifla_vvi, 3,
				jsonl_struct_open();
				print_u32_field("vf", ifla_vvi.vf, true);
				print_u32_field("vlan", ifla_vvi.vlan, false);
				print_u32_field("qos", ifla_vvi.qos, false);
				jsonl_field_open("vlan_proto", false);
				jsonl_const(eth_protos[i].val, true,
					    eth_protos[i].str, "ETH_P_???");
				jsonl_byteorder_big(
					sizeof(ifla_vvi.vlan_proto));
				jsonl_field_close();
				jsonl_struct_close());
	}

	/* IFLA_VF_INFO: IFLA_VF_BROADCAST */
	struct ifla_vf_broadcast ifla_vb;
	fill_memory(&ifla_vb.broadcast, sizeof(ifla_vb.broadcast));
	JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
				      init_vf_info_msg, print_vf_info_msg,
				      IFLA_VF_BROADCAST, pattern, ifla_vb, 2,
				      jsonl_struct_open();
				      jsonl_field_open("broadcast", true);
				      jsonl_mac(ifla_vb.broadcast,
						sizeof(ifla_vb.broadcast));
				      jsonl_field_close();
				      jsonl_struct_close());

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
