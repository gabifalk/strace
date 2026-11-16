/*
 * Check JSONL output of IFLA_XDP netlink attributes.
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

# define XLAT_MACROS_ONLY
#  include "xlat/rtnl_ifla_xdp_attrs.h"
# undef XLAT_MACROS_ONLY

# define IFLA_ATTR IFLA_XDP

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

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int32_t num = 0xabacdbcd;
	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
				   NLA_HDRLEN + sizeof(num));

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	JSONL_NESTED_NLATTR_OBJECT(fd, nlh0, hdrlen,
				   init_ifinfomsg, print_ifinfomsg,
				   IFLA_XDP_FD, pattern, num,
				   jsonl_fd(num, NULL, false));

	static const struct strval32 attach_types[] = {
		{ ARG_STR(XDP_ATTACHED_NONE) },
		{ ARG_STR(XDP_ATTACHED_MULTI) },
		{ 0x5, NULL },
		{ 0xfe, NULL },
	};

	for (size_t i = 0; i < ARRAY_SIZE(attach_types); i++) {
		const uint8_t type = attach_types[i].val;

		JSONL_NESTED_NLATTR_OBJECT(fd, nlh0, hdrlen,
					   init_ifinfomsg, print_ifinfomsg,
					   IFLA_XDP_ATTACHED, pattern, type,
					   jsonl_const(type, true,
						       attach_types[i].str,
						       "XDP_ATTACHED_???"));
	}

	static const struct strval32 xdp_flags[] = {
		{ ARG_STR(XDP_FLAGS_UPDATE_IF_NOEXIST) }
	};
	const uint32_t flags = XDP_FLAGS_UPDATE_IF_NOEXIST;
	JSONL_NESTED_NLATTR_OBJECT(fd, nlh0, hdrlen,
				   init_ifinfomsg, print_ifinfomsg,
				   IFLA_XDP_FLAGS, pattern, flags,
				   jsonl_flags(flags, xdp_flags,
					       ARRAY_SIZE(xdp_flags),
					       "XDP_FLAGS_???"));

	static const struct strval32 attrs[] = {
		{ ARG_STR(IFLA_XDP_PROG_ID) },
		{ ARG_STR(IFLA_XDP_DRV_PROG_ID) },
		{ ARG_STR(IFLA_XDP_SKB_PROG_ID) },
		{ ARG_STR(IFLA_XDP_HW_PROG_ID) },
	};

	for (size_t i = 0; i < ARRAY_SIZE(attrs); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
					       init_ifinfomsg, print_ifinfomsg,
					       attrs[i].val, attrs[i].str, NULL,
					       pattern, num,
					       jsonl_nl_bytes, 1,
					       jsonl_unsigned((uint32_t) num));
	}

	JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
				      init_ifinfomsg, print_ifinfomsg,
				      IFLA_XDP_EXPECTED_FD, pattern, num, 1,
				      jsonl_fd(num, NULL, false));

	const int exp_fd = 9;
	JSONL_NESTED_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
				      init_ifinfomsg, print_ifinfomsg,
				      IFLA_XDP_EXPECTED_FD, pattern, exp_fd, 1,
				      jsonl_fd(exp_fd, NULL, false));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
