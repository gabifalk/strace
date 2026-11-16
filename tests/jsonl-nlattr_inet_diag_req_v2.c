/*
 * Check JSONL output of INET_DIAG_REQ_* netlink attributes
 * of inet_diag_req_v2.
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
static const unsigned int hdrlen = sizeof(struct inet_diag_req_v2);
static void *nlh0;
static char pattern[4096];

static void
init_inet_diag_req_v2(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = SOCK_DIAG_BY_FAMILY,
		.nlmsg_flags = NLM_F_REQUEST
	);

	struct inet_diag_req_v2 *const req = NLMSG_DATA(nlh);
	SET_STRUCT(struct inet_diag_req_v2, req,
		.sdiag_family = AF_INET,
		.sdiag_protocol = IPPROTO_TCP,
		.id.idiag_if = ifindex_lo()
	);

	if (!inet_pton(AF_INET, address, req->id.idiag_src) ||
	    !inet_pton(AF_INET, address, req->id.idiag_dst))
		perror_msg_and_skip("inet_pton");
}

static void
print_inet_diag_req_v2(const unsigned int msg_len)
{
	static const struct strval32 nlmsg_flags[] = {
		{ ARG_STR(NLM_F_REQUEST) }
	};

	printf("{");
	jsonl_nlmsghdr_vals(msg_len, ARG_STR(SOCK_DIAG_BY_FAMILY), NULL,
			    NLM_F_REQUEST, nlmsg_flags,
			    ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	jsonl_struct_open();
	jsonl_field_const("sdiag_family", true, ARG_STR(AF_INET), NULL);
	jsonl_field_const("sdiag_protocol", false, ARG_STR(IPPROTO_TCP), NULL);
	jsonl_field_flags("idiag_ext", false, 0, NULL, 0, NULL);
	jsonl_field_flags("idiag_states", false, 0, NULL, 0, NULL);
	jsonl_field_inet_diag_sockid("id", false, address);
	jsonl_struct_close();
	printf("}");
}

# define TEST_BYTECODE(fd_, nla_data_len_, src_, slen_, ...)		\
	JSONL_NLATTR((fd_), nlh0, hdrlen,				\
		     init_inet_diag_req_v2, print_inet_diag_req_v2,	\
		     INET_DIAG_REQ_BYTECODE,				\
		     (nla_data_len_), (src_), (slen_), __VA_ARGS__)

static void
test_unk_attrs(const int fd)
{
	static const struct strval16 unk_attrs[] = {
		{ ARG_STR(INET_DIAG_REQ_NONE) },
		{ ARG_STR(INET_DIAG_REQ_SK_BPF_STORAGES) },
		{ 0x4, NULL },
		{ 0x1ace, NULL },
	};
	static const char buf[4] = { 0xde, 0xad, 0xfa, 0xce };

	for (size_t i = 0; i < ARRAY_SIZE(unk_attrs); i++) {
		JSONL_NLATTR_(fd, nlh0, hdrlen,
			      init_inet_diag_req_v2, print_inet_diag_req_v2,
			      unk_attrs[i].val, unk_attrs[i].str,
			      "INET_DIAG_REQ_???",
			      sizeof(buf), buf, sizeof(buf),
			      jsonl_bytes_value(buf, sizeof(buf), false));
	}
}

static void
print_inet_diag_bc_op(const struct inet_diag_bc_op *const op,
		      const char *const code)
{
	jsonl_struct_open();
	jsonl_field_const("code", true, op->code, code, NULL);
	jsonl_field_unsigned("yes", false, op->yes);
	jsonl_field_unsigned("no", false, op->no);
	jsonl_struct_close();
}

static void
test_inet_diag_bc_op(const int fd)
{
	static const struct inet_diag_bc_op op = {
		.code = INET_DIAG_BC_S_COND,
		.yes = 0xaf,
		.no = 0xafcd
	};
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen,
			    init_inet_diag_req_v2, print_inet_diag_req_v2,
			    INET_DIAG_REQ_BYTECODE, pattern, op,
			    print_inet_diag_bc_op(&op, "INET_DIAG_BC_S_COND"));
}

/* The operation and the separator of the element that follows it. */
static void
print_inet_diag_bc_op_next(const struct inet_diag_bc_op *const op,
			   const char *const code)
{
	print_inet_diag_bc_op(op, code);
	jsonl_nlmsg_next();
}

static void
test_inet_diag_bc_s_cond(const int fd)
{
	static const struct inet_diag_bc_op op = {
		.code = INET_DIAG_BC_S_COND,
	};
	static const struct inet_diag_hostcond cond = {
		.family = AF_UNSPEC,
		.prefix_len = 0xad,
		.port = 0xadfa
	};
	char buf[sizeof(op) + sizeof(cond)];
	memcpy(buf, &op, sizeof(op));

	const unsigned int plen = sizeof(buf) - 1;
	memcpy(buf + sizeof(op), &pattern, sizeof(cond));
	TEST_BYTECODE(fd, plen, buf, plen,
		      print_inet_diag_bc_op_next(&op, "INET_DIAG_BC_S_COND");
		      jsonl_bytes_value(buf + sizeof(op), plen - sizeof(op),
					false));

	TEST_BYTECODE(fd, sizeof(buf), buf, sizeof(buf) - 1,
		      print_inet_diag_bc_op_next(&op, "INET_DIAG_BC_S_COND");
		      jsonl_addr(jsonl_nl_data + sizeof(op)));

	memcpy(buf + sizeof(op), &cond, sizeof(cond));
	TEST_BYTECODE(fd, sizeof(buf), buf, sizeof(buf),
		      print_inet_diag_bc_op_next(&op, "INET_DIAG_BC_S_COND");
		      jsonl_struct_open();
		      jsonl_field_const("family", true, ARG_STR(AF_UNSPEC),
					NULL);
		      jsonl_field_unsigned("prefix_len", false,
					   cond.prefix_len);
		      jsonl_field_unsigned("port", false, cond.port);
		      jsonl_struct_close());
}

/* A struct inet_diag_hostcond value up to the fields of its addr value. */
static void
print_inet_diag_hostcond_open(const unsigned int family, const char *const sym)
{
	jsonl_struct_open();
	jsonl_field_const("family", true, family, sym, NULL);
	jsonl_field_unsigned("prefix_len", false, 0);
	jsonl_field_unsigned("port", false, 0);
	jsonl_field_open("addr", false);
}

static void
print_inet_diag_hostcond_close(void)
{
	jsonl_field_close();
	jsonl_struct_close();
}

static void
test_in_addr(const int fd)
{
	static const struct inet_diag_bc_op op = {
		.code = INET_DIAG_BC_S_COND,
	};
	static const struct inet_diag_hostcond cond = {
		.family = AF_INET,
	};
	struct in_addr addr;
	if (!inet_pton(AF_INET, address, &addr))
		perror_msg_and_skip("inet_pton");

	char buf[sizeof(op) + sizeof(cond) + sizeof(addr)];
	memcpy(buf, &op, sizeof(op));
	memcpy(buf + sizeof(op), &cond, sizeof(cond));

	const unsigned int plen = sizeof(buf) - 1;
	memcpy(buf + sizeof(op) + sizeof(cond), &pattern, sizeof(addr));
	TEST_BYTECODE(fd, plen, buf, plen,
		      print_inet_diag_bc_op_next(&op, "INET_DIAG_BC_S_COND");
		      print_inet_diag_hostcond_open(ARG_STR(AF_INET));
		      jsonl_bytes_value(pattern,
					plen - sizeof(op) - sizeof(cond),
					false);
		      print_inet_diag_hostcond_close());

	TEST_BYTECODE(fd, sizeof(buf), buf, sizeof(buf) - 1,
		      print_inet_diag_bc_op_next(&op, "INET_DIAG_BC_S_COND");
		      print_inet_diag_hostcond_open(ARG_STR(AF_INET));
		      jsonl_addr(jsonl_nl_data + sizeof(op) + sizeof(cond));
		      print_inet_diag_hostcond_close());

	memcpy(buf + sizeof(op) + sizeof(cond), &addr, sizeof(addr));
	TEST_BYTECODE(fd, sizeof(buf), buf, sizeof(buf),
		      print_inet_diag_bc_op_next(&op, "INET_DIAG_BC_S_COND");
		      print_inet_diag_hostcond_open(ARG_STR(AF_INET));
		      jsonl_ip_addr(ARG_STR(AF_INET), address);
		      print_inet_diag_hostcond_close());
}

static void
test_in6_addr(const int fd)
{
	static const char address6[] = "12:34:56:78:90:ab:cd:ef";
	static const struct inet_diag_bc_op op = {
		.code = INET_DIAG_BC_S_COND,
	};
	static const struct inet_diag_hostcond cond = {
		.family = AF_INET6,
	};
	struct in6_addr addr;
	if (!inet_pton(AF_INET6, address6, &addr))
		perror_msg_and_skip("inet_pton");

	char buf[sizeof(op) + sizeof(cond) + sizeof(addr)];
	memcpy(buf, &op, sizeof(op));
	memcpy(buf + sizeof(op), &cond, sizeof(cond));

	const unsigned int plen = sizeof(buf) - 1;
	memcpy(buf + sizeof(op) + sizeof(cond), &pattern, sizeof(addr));
	TEST_BYTECODE(fd, plen, buf, plen,
		      print_inet_diag_bc_op_next(&op, "INET_DIAG_BC_S_COND");
		      print_inet_diag_hostcond_open(ARG_STR(AF_INET6));
		      jsonl_bytes_value(pattern,
					plen - sizeof(op) - sizeof(cond),
					false);
		      print_inet_diag_hostcond_close());

	TEST_BYTECODE(fd, sizeof(buf), buf, sizeof(buf) - 1,
		      print_inet_diag_bc_op_next(&op, "INET_DIAG_BC_S_COND");
		      print_inet_diag_hostcond_open(ARG_STR(AF_INET6));
		      jsonl_addr(jsonl_nl_data + sizeof(op) + sizeof(cond));
		      print_inet_diag_hostcond_close());

	memcpy(buf + sizeof(op) + sizeof(cond), &addr, sizeof(addr));
	TEST_BYTECODE(fd, sizeof(buf), buf, sizeof(buf),
		      print_inet_diag_bc_op_next(&op, "INET_DIAG_BC_S_COND");
		      print_inet_diag_hostcond_open(ARG_STR(AF_INET6));
		      jsonl_ip_addr(ARG_STR(AF_INET6), address6);
		      print_inet_diag_hostcond_close());
}

static void
test_inet_diag_bc_dev_cond(const int fd)
{
	static const struct inet_diag_bc_op op = {
		.code = INET_DIAG_BC_DEV_COND,
	};
	const uint32_t ifindex = ifindex_lo();
	char buf[sizeof(op) + sizeof(ifindex)];
	memcpy(buf, &op, sizeof(op));
	memcpy(buf + sizeof(op), pattern, sizeof(ifindex));

	TEST_BYTECODE(fd, sizeof(buf) - 1, buf, sizeof(buf) - 1,
		      print_inet_diag_bc_op_next(&op, "INET_DIAG_BC_DEV_COND");
		      jsonl_bytes_value(pattern, sizeof(ifindex) - 1, false));

	TEST_BYTECODE(fd, sizeof(buf), buf, sizeof(buf) - 1,
		      print_inet_diag_bc_op_next(&op, "INET_DIAG_BC_DEV_COND");
		      jsonl_addr(jsonl_nl_data + sizeof(op)));

	memcpy(buf + sizeof(op), &ifindex, sizeof(ifindex));
	TEST_BYTECODE(fd, sizeof(buf), buf, sizeof(buf),
		      print_inet_diag_bc_op_next(&op, "INET_DIAG_BC_DEV_COND");
		      jsonl_ifindex(ifindex, "lo"));
}

static void
test_inet_diag_bc_s_le(const int fd)
{
	static const struct inet_diag_bc_op op[] = {
		{
			.code = INET_DIAG_BC_S_LE,
		},
		{
			.code = INET_DIAG_BC_DEV_COND,
			.yes = 0xaf,
			.no = 0xafcd
		}
	};

	char buf[sizeof(op)];
	memcpy(buf, op, sizeof(op[0]));
	memcpy(buf + sizeof(op[0]), pattern, sizeof(op[1]));

	const unsigned int plen = sizeof(buf) - 1;
	TEST_BYTECODE(fd, plen, buf, plen,
		      print_inet_diag_bc_op_next(&op[0], "INET_DIAG_BC_S_LE");
		      jsonl_bytes_value(buf + sizeof(op[0]),
					plen - sizeof(op[0]), false));

	TEST_BYTECODE(fd, sizeof(buf), buf, sizeof(buf) - 1,
		      print_inet_diag_bc_op_next(&op[0], "INET_DIAG_BC_S_LE");
		      jsonl_addr(jsonl_nl_data + sizeof(op[0])));

	memcpy(buf + sizeof(op[0]), &op[1], sizeof(op[1]));
	TEST_BYTECODE(fd, sizeof(buf), buf, sizeof(buf),
		      print_inet_diag_bc_op_next(&op[0], "INET_DIAG_BC_S_LE");
		      print_inet_diag_bc_op(&op[1], "INET_DIAG_BC_DEV_COND"));
}

static void
test_inet_diag_bc_mark_cond(const int fd)
{
	static const struct inet_diag_bc_op op = {
		.code = INET_DIAG_BC_MARK_COND,
	};
	static const struct inet_diag_markcond markcond = {
		.mark = 0xafbcafcd,
		.mask = 0xbafaacda
	};
	char buf[sizeof(op) + sizeof(markcond)];
	memcpy(buf, &op, sizeof(op));
	memcpy(buf + sizeof(op), pattern, sizeof(markcond));

	const unsigned int plen = sizeof(buf) - 1;
	TEST_BYTECODE(fd, plen, buf, plen,
		      print_inet_diag_bc_op_next(&op, "INET_DIAG_BC_MARK_COND");
		      jsonl_bytes_value(buf + sizeof(op), plen - sizeof(op),
					false));

	TEST_BYTECODE(fd, sizeof(buf), buf, sizeof(buf) - 1,
		      print_inet_diag_bc_op_next(&op, "INET_DIAG_BC_MARK_COND");
		      jsonl_addr(jsonl_nl_data + sizeof(op)));

	memcpy(buf + sizeof(op), &markcond, sizeof(markcond));
	TEST_BYTECODE(fd, sizeof(buf), buf, sizeof(buf),
		      print_inet_diag_bc_op_next(&op, "INET_DIAG_BC_MARK_COND");
		      jsonl_struct_open();
		      jsonl_field_unsigned("mark", true, markcond.mark);
		      jsonl_field_unsigned("mask", false, markcond.mask);
		      jsonl_struct_close());
}

static void
test_inet_diag_bc_nop(const int fd)
{
	static const struct inet_diag_bc_op op = {
		.code = INET_DIAG_BC_AUTO,
	};
	char buf[sizeof(op) + 4];
	memcpy(buf, &op, sizeof(op));
	memcpy(buf + sizeof(op), pattern, 4);

	TEST_BYTECODE(fd, sizeof(buf), buf, sizeof(buf),
		      print_inet_diag_bc_op_next(&op, "INET_DIAG_BC_AUTO");
		      jsonl_bytes_value(buf + sizeof(op),
					sizeof(buf) - sizeof(op), false));
}

static void
test_inet_diag_proto(const int fd)
{
	static const struct strval32 protos[] = {
		{ 0, "IPPROTO_IP" },
		{ 3, NULL },
		{ 6, "IPPROTO_TCP" },
		{ 144, "IPPROTO_AGGFRAG" },
		{ 145, NULL },
		{ 256, "IPPROTO_SMC" },
		{ 257, NULL },
		{ 262, "IPPROTO_MPTCP" },
		{ 0xcafeface, NULL },
	};

	for (size_t i = 0; i < ARRAY_SIZE(protos); i++) {
		JSONL_NLATTR(fd, nlh0, hdrlen,
			     init_inet_diag_req_v2, print_inet_diag_req_v2,
			     INET_DIAG_REQ_PROTOCOL,
			     sizeof(uint32_t), &protos[i].val, sizeof(uint32_t),
			     jsonl_const(protos[i].val, true, protos[i].str,
					 "IPPROTO_???"));
	}
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_SOCK_DIAG);
	nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen), NLA_HDRLEN +
			     sizeof(struct inet_diag_bc_op) +
			     sizeof(struct inet_diag_hostcond) +
			     sizeof(struct in6_addr) + DEFAULT_STRLEN);
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	test_unk_attrs(fd);
	test_inet_diag_bc_op(fd);
	test_inet_diag_bc_s_cond(fd);
	test_in_addr(fd);
	test_in6_addr(fd);
	test_inet_diag_bc_dev_cond(fd);
	test_inet_diag_bc_s_le(fd);
	test_inet_diag_bc_mark_cond(fd);
	test_inet_diag_bc_nop(fd);
	test_inet_diag_proto(fd);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
