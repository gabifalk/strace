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

# include "jsonl_nlattr_ifla.h"

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
				   jsonl_field_unsigned("vsi_mgr_id", true,
							vsi.vsi_mgr_id);
				   print_bytes_field("vsi_type_id",
						     vsi.vsi_type_id,
						     sizeof(vsi.vsi_type_id));
				   jsonl_field_unsigned("vsi_type_version",
							false,
							vsi.vsi_type_version);
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
				   jsonl_field_unsigned("vsi_mgr_id", true,
							vsi2.vsi_mgr_id);
				   print_bytes_field("vsi_type_id",
						     vsi2.vsi_type_id,
						     sizeof(vsi2.vsi_type_id));
				   jsonl_field_unsigned("vsi_type_version",
							false,
							vsi2.vsi_type_version);
				   print_bytes_field("pad", vsi2.pad,
						     sizeof(vsi2.pad));
				   jsonl_struct_close());

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
