/*
 * Check JSONL output of NFNL_SUBSYS_CTNETLINK netlink messages.
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

# include <linux/netfilter/nfnetlink.h>
# include <linux/netfilter/nfnetlink_conntrack.h>

/* A message with NLM_F_REQUEST and, when flag is not zero, that flag. */
static void
test_msg(int fd, unsigned int msg, const char *msg_sym,
	 unsigned int flag, const char *flag_sym)
{
	const struct strval32 flags[] = {
		{ ARG_STR(NLM_F_REQUEST) },
		{ flag, flag_sym }
	};

	jsonl_nfnl_test_hdr(fd, ARG_STR(NFNL_SUBSYS_CTNETLINK),
			    msg, msg_sym, "IPCTNL_MSG_CT_???",
			    NLM_F_REQUEST | flag, flags, flag ? 2 : 1);
}

static void
test_nlmsg_type(const int fd)
{
	test_msg(fd, ARG_STR(IPCTNL_MSG_CT_NEW), 0, NULL);
	test_msg(fd, 0xff, NULL, 0, NULL);
}

static void
test_nlmsg_flags(const int fd)
{
	test_msg(fd, ARG_STR(IPCTNL_MSG_CT_NEW), ARG_STR(NLM_F_REPLACE));
	test_msg(fd, ARG_STR(IPCTNL_MSG_CT_GET), ARG_STR(NLM_F_DUMP));
	test_msg(fd, ARG_STR(IPCTNL_MSG_CT_DELETE), ARG_STR(NLM_F_NONREC));
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	int fd = create_nl_socket(NETLINK_NETFILTER);

	test_nlmsg_type(fd);
	test_nlmsg_flags(fd);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
