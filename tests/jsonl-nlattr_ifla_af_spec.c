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

# include "jsonl_nlattr_ifla.h"

# include "jsonl_nlattr_ifla_af_inet6.h"

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

static void
print_inet_conf_val(const uint32_t *val, size_t idx)
{
	/* The names of these constants are expressions. */
	static const char *const syms[] = {
		"IPV4_DEVCONF_FORWARDING-1",
		"IPV4_DEVCONF_MC_FORWARDING-1",
	};

	jsonl_index_const(idx, syms, ARRAY_SIZE(syms), "IPV4_DEVCONF_???");
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
	jsonl_field_flags("flags", true, bvi->flags, bridge_vlan_flags,
			  ARRAY_SIZE(bridge_vlan_flags),
			  "BRIDGE_VLAN_INFO_???");
	jsonl_field_unsigned("vid", false, bvi->vid);
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
		msg_af_str = jsonl_xlat_sym(addrfams, j);
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
				i, jsonl_xlat_sym(addrfams, i), "AF_???",
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
