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
		.id.idiag_if = ifindex_lo()
	);

	if (!inet_pton(AF_INET, address, req->id.idiag_src) ||
	    !inet_pton(AF_INET, address, req->id.idiag_dst))
		perror_msg_and_skip("inet_pton");
}

static void
print_u_field(const char *name, unsigned int val)
{
	jsonl_field_open(name, false);
	jsonl_unsigned(val);
	jsonl_field_close();
}

static void
print_port_field(const char *name, unsigned int port, bool first)
{
	jsonl_field_open(name, first);
	jsonl_unsigned(port);
	jsonl_byteorder_big(sizeof(uint16_t));
	jsonl_field_close();
}

static void
print_addr_field(const char *name)
{
	jsonl_field_open(name, false);
	jsonl_ip_addr(ARG_STR(AF_INET), address);
	jsonl_field_close();
}

static void
print_no_flags_field(const char *name)
{
	jsonl_field_open(name, false);
	jsonl_flags(0, NULL, 0, NULL);
	jsonl_field_close();
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
	jsonl_field_open("idiag_family", true);
	jsonl_const(AF_INET, true, "AF_INET", NULL);
	jsonl_field_close();
	print_u_field("idiag_src_len", 0);
	print_u_field("idiag_dst_len", 0);
	print_no_flags_field("idiag_ext");
	jsonl_field_open("id", false);
	jsonl_struct_open();
	print_port_field("idiag_sport", 0, true);
	print_port_field("idiag_dport", 0, false);
	print_addr_field("idiag_src");
	print_addr_field("idiag_dst");
	jsonl_field_open("idiag_if", false);
	jsonl_ifindex(ifindex_lo(), "lo");
	jsonl_field_close();
	jsonl_field_open("idiag_cookie", false);
	jsonl_array_open();
	printf("{");
	jsonl_unsigned(0);
	printf("}, {");
	jsonl_unsigned(0);
	printf("}");
	jsonl_array_close();
	jsonl_field_close();
	jsonl_struct_close();
	jsonl_field_close();
	print_no_flags_field("idiag_states");
	print_u_field("idiag_dbs", 0);
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
