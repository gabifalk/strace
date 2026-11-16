/*
 * Check JSONL output of DCB_ATTR_* netlink attributes of dcbmsg.
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

# include <linux/dcbnl.h>

static void
init_dcbmsg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = RTM_GETDCB,
		.nlmsg_flags = NLM_F_DUMP
	);

	struct dcbmsg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct dcbmsg, msg,
		.dcb_family = AF_UNIX,
		.cmd = DCB_CMD_UNDEFINED
	);
}

static void
print_dcbmsg(const unsigned int msg_len)
{
	static const struct strval32 nlmsg_flags[] = {
		{ ARG_STR(NLM_F_DUMP) }
	};

	printf("{");
	jsonl_nlmsghdr_vals(msg_len, ARG_STR(RTM_GETDCB), NULL,
			    NLM_F_DUMP, nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	jsonl_struct_open();
	jsonl_field_open("dcb_family", true);
	jsonl_const(AF_UNIX, true, "AF_UNIX", NULL);
	jsonl_field_close();
	jsonl_field_open("cmd", false);
	jsonl_const(DCB_CMD_UNDEFINED, true, "DCB_CMD_UNDEFINED", NULL);
	jsonl_field_close();
	jsonl_struct_close();
	printf("}");
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	const unsigned int hdrlen = sizeof(struct dcbmsg);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen), NLA_HDRLEN + 4);

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	const unsigned int nla_type = 0xffff & NLA_TYPE_MASK;
	JSONL_NLATTR_(fd, nlh0, hdrlen, init_dcbmsg, print_dcbmsg,
		      nla_type, NULL, "DCB_ATTR_???",
		      4, pattern, 4,
		      jsonl_bytes_value(pattern, 4, false));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
