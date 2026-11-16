/*
 * Check JSONL output of INET_DIAG_REQ_* netlink attributes
 * of inet_diag_req.
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

# include <arpa/inet.h>
# include <netinet/tcp.h>
# include <linux/inet_diag.h>
# include <linux/sock_diag.h>

static const char address[] = "10.11.12.13";

static void
init_inet_diag_req(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = TCPDIAG_GETSOCK,
		.nlmsg_flags = NLM_F_REQUEST
	);

	struct inet_diag_req *const req = NLMSG_DATA(nlh);
	SET_STRUCT(struct inet_diag_req, req,
		.idiag_family = AF_INET,
		.idiag_ext = 1 << (INET_DIAG_TOS - 1),
		.idiag_states = 1 << TCP_LAST_ACK,
		.id.idiag_if = ifindex_lo()
	);

	if (!inet_pton(AF_INET, address, req->id.idiag_src) ||
	    !inet_pton(AF_INET, address, req->id.idiag_dst))
		perror_msg_and_skip("inet_pton");
}

static void
print_inet_diag_req(const unsigned int msg_len)
{
	static const struct strval32 nlmsg_flags[] = {
		{ ARG_STR(NLM_F_REQUEST) }
	};

	printf("{");
	jsonl_nlmsghdr_vals(msg_len, ARG_STR(TCPDIAG_GETSOCK), NULL,
			    NLM_F_REQUEST, nlmsg_flags,
			    ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	jsonl_struct_open();
	jsonl_field_const("idiag_family", true, AF_INET, "AF_INET", NULL);
	jsonl_field_unsigned("idiag_src_len", false, 0);
	jsonl_field_unsigned("idiag_dst_len", false, 0);
	jsonl_field_flag("idiag_ext", false, 1 << (INET_DIAG_TOS - 1),
			 "1<<(INET_DIAG_TOS-1)");
	jsonl_field_inet_diag_sockid("id", false, address);
	jsonl_field_flag("idiag_states", false, 1 << TCP_LAST_ACK,
			 "1<<TCP_LAST_ACK");
	jsonl_field_unsigned("idiag_dbs", false, 0);
	jsonl_struct_close();
	printf("}");
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_SOCK_DIAG);
	const unsigned int hdrlen = sizeof(struct inet_diag_req);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen), NLA_HDRLEN + 4);

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	JSONL_NLATTR_(fd, nlh0, hdrlen,
		      init_inet_diag_req, print_inet_diag_req,
		      INET_DIAG_REQ_PROTOCOL + 1, NULL, "INET_DIAG_REQ_???",
		      4, pattern, 4,
		      jsonl_bytes_value(pattern, 4, false));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
