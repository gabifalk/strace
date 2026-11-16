/*
 * Check JSONL output of IFLA_PORT_SELF netlink attributes.
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

# define IFLA_ATTR IFLA_PORT_SELF

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

static void
print_u8_field(const char *name, uint8_t val, bool first)
{
	jsonl_field_open(name, first);
	jsonl_unsigned(val);
	jsonl_field_close();
}

static void
print_bytes_field(const char *name, const void *p, size_t len)
{
	jsonl_field_open(name, false);
	jsonl_bytes_value(p, len, false);
	jsonl_field_close();
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen), 2 * NLA_HDRLEN + 8);

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	const uint32_t num = 0xabacdbcd;
	JSONL_NESTED_NLATTR_OBJECT(fd, nlh0, hdrlen,
				   init_ifinfomsg, print_ifinfomsg,
				   IFLA_PORT_VF, pattern, num,
				   jsonl_unsigned(num));

	static const struct ifla_port_vsi vsi = {
		.vsi_mgr_id = 0xab,
		.vsi_type_id = { 'a', 'b', 'c' },
		.vsi_type_version = 0xef
	};
	JSONL_NESTED_NLATTR_OBJECT(fd, nlh0, hdrlen,
				   init_ifinfomsg, print_ifinfomsg,
				   IFLA_PORT_VSI_TYPE, pattern, vsi,
				   jsonl_struct_open();
				   print_u8_field("vsi_mgr_id", vsi.vsi_mgr_id,
						  true);
				   print_bytes_field("vsi_type_id",
						     vsi.vsi_type_id,
						     sizeof(vsi.vsi_type_id));
				   print_u8_field("vsi_type_version",
						  vsi.vsi_type_version, false);
				   jsonl_struct_close());

	static const struct ifla_port_vsi vsi2 = {
		.vsi_mgr_id = 0xab,
		.vsi_type_id = { 10, 0, 255 },
		.vsi_type_version = 0xef,
		.pad = { 0, 1, 2 },
	};
	JSONL_NESTED_NLATTR_OBJECT(fd, nlh0, hdrlen,
				   init_ifinfomsg, print_ifinfomsg,
				   IFLA_PORT_VSI_TYPE, pattern, vsi2,
				   jsonl_struct_open();
				   print_u8_field("vsi_mgr_id", vsi2.vsi_mgr_id,
						  true);
				   print_bytes_field("vsi_type_id",
						     vsi2.vsi_type_id,
						     sizeof(vsi2.vsi_type_id));
				   print_u8_field("vsi_type_version",
						  vsi2.vsi_type_version, false);
				   print_bytes_field("pad", vsi2.pad,
						     sizeof(vsi2.pad));
				   jsonl_struct_close());

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
