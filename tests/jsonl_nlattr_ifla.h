/*
 * The JSONL counterpart of nlattr_ifla.h.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef STRACE_TESTS_JSONL_NLATTR_IFLA_H
# define STRACE_TESTS_JSONL_NLATTR_IFLA_H

# include "jsonl_netlink.h"

/*
 * The test has to include the headers that define struct ifinfomsg,
 * IFF_UP and ARPHRD_LOOPBACK, and may define IFLA_ATTR, the attribute
 * that encloses the tested attributes, IFLA_AF and IFLA_AF_STR.
 */
# ifndef IFLA_AF
#  define IFLA_AF AF_UNIX
# endif
# ifndef IFLA_AF_STR
#  define IFLA_AF_STR "AF_UNIX"
# endif

static const unsigned int hdrlen = sizeof(struct ifinfomsg);

/*
 * Initialize an RTM_GETLINK message with a struct ifinfomsg of interface lo
 * followed, when IFLA_ATTR is defined, by the header of that attribute.
 */
static inline void
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

# ifdef IFLA_ATTR
	struct nlattr *const nla = NLMSG_ATTR(nlh, sizeof(*msg));
	SET_STRUCT(struct nlattr, nla,
		.nla_len = msg_len - NLMSG_SPACE(hdrlen),
		.nla_type = IFLA_ATTR
	);
# endif
}

/* The netlink header and the struct ifinfomsg. */
static inline void
print_ifinfomsg_hdr(const unsigned int msg_len)
{
	static const struct strval32 ifi_flags[] = {
		{ ARG_STR(IFF_UP) }
	};

	jsonl_nlmsghdr_dump_open(msg_len, ARG_STR(RTM_GETLINK));
	jsonl_struct_open();
	jsonl_field_const("ifi_family", true, IFLA_AF, IFLA_AF_STR, "AF_???");
	jsonl_field_const("ifi_type", false, ARPHRD_LOOPBACK, "ARPHRD_LOOPBACK",
			  NULL);
	jsonl_field_ifindex("ifi_index", false, ifindex_lo(), "lo");
	jsonl_field_flags("ifi_flags", false, IFF_UP, ifi_flags,
			  ARRAY_SIZE(ifi_flags), NULL);
	jsonl_field_hex("ifi_change", false, 0);
	jsonl_struct_close();
	printf("}");
}

/* Likewise, followed by the enclosing IFLA_ATTR attribute, if any. */
static inline void
print_ifinfomsg(const unsigned int msg_len)
{
	print_ifinfomsg_hdr(msg_len);
# ifdef IFLA_ATTR
	jsonl_nlattr_nest_open(msg_len - NLMSG_SPACE(hdrlen),
			       IFLA_ATTR, STRINGIFY_VAL(IFLA_ATTR), NULL);
# endif
}

#endif /* !STRACE_TESTS_JSONL_NLATTR_IFLA_H */
