/*
 * Check JSONL output of IFLA_PROTINFO netlink attributes.
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

# include <netinet/in.h>
# include <linux/if_arp.h>
# include <linux/if_link.h>

# include "xlat.h"
# include "xlat/addrfams.h"

static uint8_t af;
static const char *af_str;

# define IFLA_ATTR IFLA_PROTINFO
# define IFLA_AF af
# define IFLA_AF_STR af_str

# include "jsonl_nlattr_ifla.h"

# include "jsonl_nlattr_ifla_af_inet6.h"

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
				   NLA_HDRLEN * 2 + 42);

	static char buf[256];
	fill_memory_ex(buf, sizeof(buf), 32, 224);

	/* Unsupported address families */
	static const uint8_t skip_af[] = { AF_BRIDGE, AF_INET6 };
	size_t pos = 0;
	for (size_t i = 0; i < 256; i++) {
		if (pos < ARRAY_SIZE(skip_af) && i == skip_af[pos]) {
			pos += 1;
			continue;
		}

		af = i;
		af_str = jsonl_xlat_sym(addrfams, i);
		JSONL_NLATTR(fd, nlh0, hdrlen,
			     init_ifinfomsg, print_ifinfomsg_hdr,
			     IFLA_PROTINFO, 42, buf, 42,
			     jsonl_bytes_value(buf, 32, true));
	}

	/* AF_BRIDGE is handled by jsonl-nlattr_ifla_brport */

	/* AF_INET6 */
	af = AF_INET6;
	af_str = "AF_INET6";
	check_ifla_af_inet6(fd, nlh0, hdrlen,
			    init_ifinfomsg, print_ifinfomsg, buf, 1);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
