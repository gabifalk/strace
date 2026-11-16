/*
 * Check JSONL output of MDBA_ROUTER_PORT netlink attribute of br_port_msg.
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

# include <linux/if_bridge.h>

static const unsigned int hdrlen = sizeof(struct br_port_msg);

static void
init_br_port_msg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = RTM_GETMDB,
		.nlmsg_flags = NLM_F_DUMP
	);

	struct br_port_msg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct br_port_msg, msg,
		.family = AF_UNIX,
		.ifindex = ifindex_lo()
	);

	struct nlattr *nla = NLMSG_ATTR(nlh, sizeof(*msg));
	SET_STRUCT(struct nlattr, nla,
		.nla_len = msg_len - NLMSG_SPACE(hdrlen),
		.nla_type = MDBA_ROUTER
	);
}

static void
print_br_port_msg(const unsigned int msg_len)
{
	jsonl_nl_print_br_port_msg(msg_len);
	jsonl_nlattr_nest_open(msg_len - NLMSG_SPACE(hdrlen),
			       ARG_STR(MDBA_ROUTER), NULL);
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const uint32_t ifindex = ifindex_lo();
	const uint8_t type = MDB_RTR_TYPE_DISABLED;
	static const struct nlattr nla = {
		.nla_len = NLA_HDRLEN + sizeof(type),
		.nla_type = MDBA_ROUTER_PATTR_TYPE
	};
	char buf[NLMSG_ALIGN(sizeof(ifindex)) + NLA_HDRLEN + sizeof(type)];

	uint32_t timer = 0xdead;
	const long clk_tck = sysconf(_SC_CLK_TCK);
	if (clk_tck > 0)
		timer *= clk_tck;

	struct nlattr nla_timer = {
		.nla_len = NLA_HDRLEN + sizeof(timer),
	};
	char buf_timer[NLMSG_ALIGN(sizeof(ifindex)) + NLA_HDRLEN
		       + sizeof(timer)];

	const int fd = create_nl_socket(NETLINK_ROUTE);

	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
				   2 * NLA_HDRLEN + sizeof(buf_timer));

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	JSONL_NESTED_NLATTR_OBJECT(fd, nlh0, hdrlen,
				   init_br_port_msg, print_br_port_msg,
				   MDBA_ROUTER_PORT, pattern, ifindex,
				   jsonl_ifindex(ifindex, "lo"));

	/* The interface index is followed by MDBA_ROUTER_PATTR_* attributes. */
	memcpy(buf, &ifindex, sizeof(ifindex));
	memcpy(buf + NLMSG_ALIGN(sizeof(ifindex)), &nla, sizeof(nla));
	memcpy(buf + NLMSG_ALIGN(sizeof(ifindex)) + NLA_HDRLEN,
	       &type, sizeof(type));
	JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
			     init_br_port_msg, print_br_port_msg,
			     MDBA_ROUTER_PORT, "MDBA_ROUTER_PORT", NULL,
			     sizeof(buf), buf, sizeof(buf), 1,
			     jsonl_ifindex(ifindex, "lo");
			     jsonl_nlmsg_next();
			     jsonl_nlattr_open(nla.nla_len,
					       ARG_STR(MDBA_ROUTER_PATTR_TYPE),
					       NULL);
			     jsonl_const(type, true, "MDB_RTR_TYPE_DISABLED",
					 NULL);
			     jsonl_nlattr_close());

	/* timers */
	static const struct strval32 pattrs[] = {
		{ ARG_STR(MDBA_ROUTER_PATTR_TIMER) },
		{ ARG_STR(MDBA_ROUTER_PATTR_INET_TIMER) },
		{ ARG_STR(MDBA_ROUTER_PATTR_INET6_TIMER) },
	};

	memcpy(buf_timer, &ifindex, sizeof(ifindex));
	memcpy(buf_timer + NLMSG_ALIGN(sizeof(ifindex)) + NLA_HDRLEN,
	       &timer, sizeof(timer));

	for (size_t i = 0; i < ARRAY_SIZE(pattrs); i++) {
		nla_timer.nla_type = pattrs[i].val;
		memcpy(buf_timer + NLMSG_ALIGN(sizeof(ifindex)),
		       &nla_timer, sizeof(nla_timer));

		JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
				     init_br_port_msg, print_br_port_msg,
				     MDBA_ROUTER_PORT, "MDBA_ROUTER_PORT", NULL,
				     sizeof(buf_timer), buf_timer,
				     sizeof(buf_timer), 1,
				     jsonl_ifindex(ifindex, "lo");
				     jsonl_nlmsg_next();
				     jsonl_nlattr_open(nla_timer.nla_len,
						       pattrs[i].val,
						       pattrs[i].str, NULL);
				     jsonl_ticks(timer);
				     jsonl_nlattr_close());
	}

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
