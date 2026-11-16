/*
 * Check JSONL output of IFLA_PROTO_DOWN_REASON netlink attributes.
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
#  include "xlat/rtnl_ifla_proto_down_reason_attrs.h"
# undef XLAT_MACROS_ONLY

# define IFLA_ATTR IFLA_PROTO_DOWN_REASON

# include "jsonl_nlattr_ifla.h"

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const uint32_t num = 0xabacdbcd;
	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
				   NLA_HDRLEN + sizeof(num));

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	static const struct strval32 inv_attrs[] = {
		{ ARG_STR(IFLA_PROTO_DOWN_REASON_UNSPEC) },
		{ 0x3, NULL },
		{ 0xfe, NULL },
	};

	for (size_t i = 0; i < ARRAY_SIZE(inv_attrs); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
					       init_ifinfomsg, print_ifinfomsg,
					       inv_attrs[i].val,
					       inv_attrs[i].str,
					       "IFLA_PROTO_DOWN_REASON_???",
					       pattern, num,
					       jsonl_nl_bytes, 1,
					       jsonl_nl_bytes(&num,
							      sizeof(num)));
	}

	static const struct strval32 attrs[] = {
		{ ARG_STR(IFLA_PROTO_DOWN_REASON_MASK) },
		{ ARG_STR(IFLA_PROTO_DOWN_REASON_VALUE) },
	};

	for (size_t i = 0; i < ARRAY_SIZE(attrs); i++) {
		JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
					       init_ifinfomsg, print_ifinfomsg,
					       attrs[i].val, attrs[i].str, NULL,
					       pattern, num,
					       jsonl_nl_bytes, 1,
					       jsonl_hex(num));
	}

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
