/*
 * Check JSONL output of NFNL_SUBSYS_CTHELPER netlink messages.
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
# include <linux/netfilter/nfnetlink_cthelper.h>

# define NFNL_SHIFT 8

/* The fields of the nlmsg_type value of a netfilter subsystem message. */
static void
jsonl_nfnl_msg_type(unsigned int type, const char *subsys_sym,
		    const char *msg_sym, const char *msg_pattern)
{
	const unsigned int subsys = type >> NFNL_SHIFT;
	const unsigned int msg = type & ((1U << NFNL_SHIFT) - 1);

	jsonl_flags_open(type);
	printf("{\"type\": \"shift\", \"raw\": ");
	jsonl_xlat_raw(subsys << NFNL_SHIFT);
	printf(", \"value\": {");
	jsonl_const(subsys, true, subsys_sym, "NFNL_SUBSYS_???");
	printf("}, \"shift\": \"%u\"}, {", NFNL_SHIFT);
	jsonl_const(msg, true, msg_sym, msg_pattern);
	printf("}");
	jsonl_flags_close();
}

/*
 * Send a netfilter subsystem message that consists of a header alone
 * and print its event; the names of flags are taken from known.
 */
static void
jsonl_nfnl_test_hdr(int fd, unsigned int subsys, const char *subsys_sym,
		    unsigned int msg, const char *msg_sym,
		    const char *msg_pattern, unsigned int flags,
		    const struct strval32 *known, size_t n)
{
	const unsigned int type = subsys << NFNL_SHIFT | msg;
	const struct nlmsghdr nlh = {
		.nlmsg_len = sizeof(nlh),
		.nlmsg_type = type,
		.nlmsg_flags = flags
	};

	jsonl_nl_sendto(fd, &nlh, nlh.nlmsg_len);
	jsonl_nl_sendto_open(fd);
	jsonl_nlmsghdr_type_open(nlh.nlmsg_len);
	jsonl_nfnl_msg_type(type, subsys_sym, msg_sym, msg_pattern);
	jsonl_nlmsghdr_type_close(flags, known, n, 0, 0);
	jsonl_nl_sendto_close();
}

/* A message with NLM_F_REQUEST and, when flag is not zero, that flag. */
static void
test_msg(int fd, unsigned int msg, const char *msg_sym,
	 unsigned int flag, const char *flag_sym)
{
	const struct strval32 flags[] = {
		{ ARG_STR(NLM_F_REQUEST) },
		{ flag, flag_sym }
	};

	jsonl_nfnl_test_hdr(fd, ARG_STR(NFNL_SUBSYS_CTHELPER),
			    msg, msg_sym, "NFNL_MSG_CTHELPER_???",
			    NLM_F_REQUEST | flag, flags, flag ? 2 : 1);
}

static void
test_nlmsg_type(const int fd)
{
	test_msg(fd, ARG_STR(NFNL_MSG_CTHELPER_NEW), 0, NULL);
	test_msg(fd, 0xff, NULL, 0, NULL);
}

static void
test_nlmsg_flags(const int fd)
{
	test_msg(fd, ARG_STR(NFNL_MSG_CTHELPER_NEW), ARG_STR(NLM_F_REPLACE));
	test_msg(fd, ARG_STR(NFNL_MSG_CTHELPER_GET), ARG_STR(NLM_F_DUMP));
	test_msg(fd, ARG_STR(NFNL_MSG_CTHELPER_DEL), ARG_STR(NLM_F_NONREC));
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
