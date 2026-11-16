/*
 * Check JSONL output of IFLA_BRPORT_* netlink attributes.
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
# include <linux/if_link.h>

# define IFLA_ATTR IFLA_PROTINFO
# define IFLA_AF AF_BRIDGE
# define IFLA_AF_STR "AF_BRIDGE"

# include "jsonl_nlattr_ifla.h"

static void
print_bridge_id(const struct ifla_bridge_id *const id)
{
	jsonl_struct_open();
	jsonl_field_open("prio", true);
	jsonl_array_open();
	for (size_t i = 0; i < ARRAY_SIZE(id->prio); i++) {
		printf("%s{", i ? ", " : "");
		jsonl_unsigned(id->prio[i]);
		printf("}");
	}
	jsonl_array_close();
	jsonl_field_close();
	jsonl_field_open("addr", false);
	jsonl_mac(id->addr, sizeof(id->addr));
	jsonl_field_close();
	jsonl_struct_close();
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
				   NLA_HDRLEN * 2 + 42);

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	/* Unknown, unhandled, unsupported */
	static const struct strval32 un_attrs[] = {
		{ ARG_STR(IFLA_BRPORT_UNSPEC) },
		{ ARG_STR(IFLA_BRPORT_FLUSH) },
		{ ARG_STR(IFLA_BRPORT_PAD) },
		{ 0x2e, NULL },
		{ 0xbad, NULL },
	};
	for (size_t i = 0; i < ARRAY_SIZE(un_attrs); i++) {
		JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
				     init_ifinfomsg, print_ifinfomsg,
				     un_attrs[i].val, un_attrs[i].str,
				     "IFLA_BRPORT_???",
				     42, pattern, 42, 1,
				     jsonl_bytes_value(pattern, 32, true));
	}

	/* u8 attrs */
	static const struct strval32 u8_attrs[] = {
		{ ARG_STR(IFLA_BRPORT_STATE) },
		{ ARG_STR(IFLA_BRPORT_MODE) },
		{ ARG_STR(IFLA_BRPORT_GUARD) },
		{ ARG_STR(IFLA_BRPORT_PROTECT) },
		{ ARG_STR(IFLA_BRPORT_FAST_LEAVE) },
		{ ARG_STR(IFLA_BRPORT_LEARNING) },
		{ ARG_STR(IFLA_BRPORT_UNICAST_FLOOD) },
		{ ARG_STR(IFLA_BRPORT_PROXYARP) },
		{ ARG_STR(IFLA_BRPORT_LEARNING_SYNC) },
		{ ARG_STR(IFLA_BRPORT_PROXYARP_WIFI) },
		{ ARG_STR(IFLA_BRPORT_TOPOLOGY_CHANGE_ACK) },
		{ ARG_STR(IFLA_BRPORT_CONFIG_PENDING) },
		{ ARG_STR(IFLA_BRPORT_MULTICAST_ROUTER) },
		{ ARG_STR(IFLA_BRPORT_MCAST_FLOOD) },
		{ ARG_STR(IFLA_BRPORT_MCAST_TO_UCAST) },
		{ ARG_STR(IFLA_BRPORT_VLAN_TUNNEL) },
		{ ARG_STR(IFLA_BRPORT_BCAST_FLOOD) },
		{ ARG_STR(IFLA_BRPORT_NEIGH_SUPPRESS) },
		{ ARG_STR(IFLA_BRPORT_ISOLATED) },
		{ ARG_STR(IFLA_BRPORT_MRP_RING_OPEN) },
		{ ARG_STR(IFLA_BRPORT_MRP_IN_OPEN) },
		{ ARG_STR(IFLA_BRPORT_LOCKED) },
		{ ARG_STR(IFLA_BRPORT_MAB) },
		{ ARG_STR(IFLA_BRPORT_NEIGH_VLAN_SUPPRESS) },
		{ ARG_STR(IFLA_BRPORT_NEIGH_FORWARD_GRAT) },
	};
	void *nlh_u8 = midtail_alloc(NLMSG_SPACE(hdrlen),
				     NLA_HDRLEN * 2 + sizeof(uint8_t));
	for (size_t i = 0; i < ARRAY_SIZE(u8_attrs); i++) {
		jsonl_check_u8_nlattr(fd, nlh_u8, hdrlen,
				      init_ifinfomsg, print_ifinfomsg,
				      u8_attrs[i].val, u8_attrs[i].str, NULL,
				      pattern, 1);
	}

	/* u16 attrs */
	static const struct strval32 u16_attrs[] = {
		{ ARG_STR(IFLA_BRPORT_PRIORITY) },
		{ ARG_STR(IFLA_BRPORT_DESIGNATED_PORT) },
		{ ARG_STR(IFLA_BRPORT_DESIGNATED_COST) },
		{ ARG_STR(IFLA_BRPORT_ID) },
		{ ARG_STR(IFLA_BRPORT_NO) },
	};
	void *nlh_u16 = midtail_alloc(NLMSG_SPACE(hdrlen),
				      NLA_HDRLEN * 2 + sizeof(uint16_t));
	for (size_t i = 0; i < ARRAY_SIZE(u16_attrs); i++) {
		jsonl_check_u16_nlattr(fd, nlh_u16, hdrlen,
				       init_ifinfomsg, print_ifinfomsg,
				       u16_attrs[i].val, u16_attrs[i].str, NULL,
				       pattern, 1);
	}

	/* x16 attrs */
	jsonl_check_x16_nlattr(fd, nlh_u16, hdrlen,
			       init_ifinfomsg, print_ifinfomsg,
			       ARG_STR(IFLA_BRPORT_GROUP_FWD_MASK), NULL,
			       pattern, 1);

	/* u32 attrs */
	static const struct strval32 u32_attrs[] = {
		{ ARG_STR(IFLA_BRPORT_COST) },
		{ ARG_STR(IFLA_BRPORT_MCAST_EHT_HOSTS_LIMIT) },
		{ ARG_STR(IFLA_BRPORT_MCAST_EHT_HOSTS_CNT) },
		{ ARG_STR(IFLA_BRPORT_MCAST_N_GROUPS) },
		{ ARG_STR(IFLA_BRPORT_MCAST_MAX_GROUPS) },
		{ ARG_STR(IFLA_BRPORT_BACKUP_NHID) },
	};
	void *nlh_u32 = midtail_alloc(NLMSG_SPACE(hdrlen),
				      NLA_HDRLEN * 2 + sizeof(uint32_t));
	for (size_t i = 0; i < ARRAY_SIZE(u32_attrs); i++) {
		jsonl_check_u32_nlattr(fd, nlh_u32, hdrlen,
				       init_ifinfomsg, print_ifinfomsg,
				       u32_attrs[i].val, u32_attrs[i].str, NULL,
				       pattern, 1);
	}

	/* clock_t attrs */
	static const struct strval32 c_t_attrs[] = {
		{ ARG_STR(IFLA_BRPORT_MESSAGE_AGE_TIMER) },
		{ ARG_STR(IFLA_BRPORT_FORWARD_DELAY_TIMER) },
		{ ARG_STR(IFLA_BRPORT_HOLD_TIMER) },
	};
	void *nlh_c_t = midtail_alloc(NLMSG_SPACE(hdrlen),
				      NLA_HDRLEN * 2 + sizeof(uint64_t) + 1);
	for (size_t i = 0; i < ARRAY_SIZE(c_t_attrs); i++) {
		jsonl_check_clock_t_nlattr(fd, nlh_c_t, hdrlen,
					   init_ifinfomsg, print_ifinfomsg,
					   c_t_attrs[i].val, c_t_attrs[i].str,
					   NULL, 1);
	}

	/* struct ifla_bridge_id attrs */
	static const struct ifla_bridge_id id = {
		.prio = { 0xab, 0xcd },
		.addr = { 0xab, 0xcd, 0xef, 0xac, 0xbc, 0xcd }
	};
	static const struct strval32 id_attrs[] = {
		{ ARG_STR(IFLA_BRPORT_ROOT_ID) },
		{ ARG_STR(IFLA_BRPORT_BRIDGE_ID) },
	};
	void *nlh_id = midtail_alloc(NLMSG_SPACE(hdrlen),
				     NLA_HDRLEN * 2 + sizeof(id));
	for (size_t i = 0; i < ARRAY_SIZE(id_attrs); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh_id, hdrlen,
					       init_ifinfomsg, print_ifinfomsg,
					       id_attrs[i].val, id_attrs[i].str,
					       NULL, pattern, id,
					       jsonl_nl_bytes, 1,
					       print_bridge_id(&id));
	}

	/* ifindex attrs */
	uint32_t ifidx = 0xbadc0ded;
	JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh_u32, hdrlen,
				      init_ifinfomsg, print_ifinfomsg,
				      IFLA_BRPORT_BACKUP_PORT, pattern, ifidx,
				      1, jsonl_ifindex(ifidx, NULL));
	ifidx = ifindex_lo();
	JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh_u32, hdrlen,
				      init_ifinfomsg, print_ifinfomsg,
				      IFLA_BRPORT_BACKUP_PORT, pattern, ifidx,
				      1, jsonl_ifindex(ifidx, "lo"));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
