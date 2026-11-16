/*
 * Check JSONL output of netlink attribute decoding.
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

# include <netinet/tcp.h>
# include <linux/sock_diag.h>
# include <linux/unix_diag.h>

enum { UNIX_DIAG_FIRST_UNUSED = __UNIX_DIAG_MAX };

struct msg {
	struct nlmsghdr nlh;
	struct unix_diag_msg udm;
};

static const struct msg c_msg = {
	.nlh = {
		.nlmsg_len = sizeof(struct msg),
		.nlmsg_type = SOCK_DIAG_BY_FAMILY,
		.nlmsg_flags = NLM_F_DUMP
	},
	.udm = {
		.udiag_family = AF_UNIX,
		.udiag_type = SOCK_STREAM,
		.udiag_state = TCP_FIN_WAIT1
	}
};

static void
field_const(const char *name, uint64_t val, const char *sym, bool first)
{
	jsonl_field_open(name, first);
	jsonl_const(val, true, sym, NULL);
	jsonl_field_close();
}

/* The event up to the fields of the element that follows the message. */
static void
msg_open(int fd)
{
	static const struct strval32 nlmsg_flags[] = {
		{ ARG_STR(NLM_F_DUMP) }
	};

	jsonl_nl_sendto_open(fd);
	jsonl_array_open();
	printf("{");
	jsonl_nlmsghdr_vals(jsonl_nl_len, c_msg.nlh.nlmsg_type,
			    "SOCK_DIAG_BY_FAMILY", NULL, c_msg.nlh.nlmsg_flags,
			    nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	jsonl_struct_open();
	field_const("udiag_family", c_msg.udm.udiag_family, "AF_UNIX", true);
	field_const("udiag_type", c_msg.udm.udiag_type, "SOCK_STREAM", false);
	field_const("udiag_state", c_msg.udm.udiag_state, "TCP_FIN_WAIT1",
		    false);
	jsonl_field_open("udiag_ino", false);
	jsonl_unsigned(c_msg.udm.udiag_ino);
	jsonl_field_close();
	jsonl_field_open("udiag_cookie", false);
	jsonl_array_open();
	printf("{");
	jsonl_unsigned(c_msg.udm.udiag_cookie[0]);
	printf("}, {");
	jsonl_unsigned(c_msg.udm.udiag_cookie[1]);
	printf("}");
	jsonl_array_close();
	jsonl_field_close();
	jsonl_struct_close();
	printf("}, {");
}

static void
msg_close(void)
{
	printf("}");
	jsonl_array_close();
	jsonl_nl_sendto_close();
}

/* The fields of a struct nlattr whose type is a UNIX_DIAG_* constant. */
static void
nla_hdr(const struct nlattr *nla, const char *sym)
{
	jsonl_nlattr_hdr(nla->nla_len, nla->nla_type, sym, "UNIX_DIAG_???");
}

static void
test_nlattr(const int fd)
{
	struct msg *msg;
	struct nlattr *nla;
	unsigned int msg_len;

	/* fetch fail: len < sizeof(struct nlattr) */
	msg_len = NLMSG_SPACE(sizeof(msg->udm)) + 2;
	msg = tail_memdup(&c_msg, msg_len);
	memcpy(&msg->nlh.nlmsg_len, &msg_len, sizeof(msg_len));
	nla = NLMSG_ATTR(msg, sizeof(msg->udm));
	memcpy(nla, "12", 2);
	jsonl_nl_sendto(fd, msg, msg_len);
	msg_open(fd);
	jsonl_bytes_value(nla, 2, false);
	msg_close();

	/* fetch fail: short read */
	msg_len = NLMSG_SPACE(sizeof(msg->udm)) + sizeof(*nla);
	msg = tail_memdup(&c_msg, msg_len - 1);
	memcpy(&msg->nlh.nlmsg_len, &msg_len, sizeof(msg_len));
	jsonl_nl_sendto(fd, msg, msg_len);
	msg_open(fd);
	jsonl_addr((void *) msg + NLMSG_SPACE(sizeof(msg->udm)));
	msg_close();

	/* one struct nlattr */
	msg_len = NLMSG_SPACE(sizeof(msg->udm)) + sizeof(*nla);
	msg = tail_memdup(&c_msg, msg_len);
	memcpy(&msg->nlh.nlmsg_len, &msg_len, sizeof(msg_len));
	nla = NLMSG_ATTR(msg, sizeof(msg->udm));
	*nla = (struct nlattr) {
		.nla_len = sizeof(*nla),
		.nla_type = UNIX_DIAG_NAME
	};
	jsonl_nl_sendto(fd, msg, msg_len);
	msg_open(fd);
	nla_hdr(nla, "UNIX_DIAG_NAME");
	msg_close();

	/* one struct nlattr with nla_len out of msg_len bounds */
	nla->nla_len += 8;
	jsonl_nl_sendto(fd, msg, msg_len);
	msg_open(fd);
	nla_hdr(nla, "UNIX_DIAG_NAME");
	msg_close();

	/* one struct nlattr and some data */
	msg_len = NLMSG_SPACE(sizeof(msg->udm)) + NLA_HDRLEN + 4;
	msg = tail_memdup(&c_msg, msg_len);
	memcpy(&msg->nlh.nlmsg_len, &msg_len, sizeof(msg_len));
	nla = NLMSG_ATTR(msg, sizeof(msg->udm));
	*nla = (struct nlattr) {
		.nla_len = NLA_HDRLEN + 4,
		.nla_type = UNIX_DIAG_FIRST_UNUSED
	};
	memcpy(RTA_DATA(nla), "1234", 4);
	jsonl_nl_sendto(fd, msg, msg_len);
	msg_open(fd);
	jsonl_nlattr_open(nla->nla_len, nla->nla_type, NULL, "UNIX_DIAG_???");
	jsonl_bytes_value(RTA_DATA(nla), 4, false);
	jsonl_nlattr_close();
	msg_close();

	/* one struct nlattr and fetch fail of the second struct nlattr */
	msg_len = NLMSG_SPACE(sizeof(msg->udm)) + NLA_HDRLEN + 2;
	msg = tail_memdup(&c_msg, msg_len);
	memcpy(&msg->nlh.nlmsg_len, &msg_len, sizeof(msg_len));
	nla = NLMSG_ATTR(msg, sizeof(msg->udm));
	SET_STRUCT(struct nlattr, nla,
		.nla_len = NLA_HDRLEN,
		.nla_type = UNIX_DIAG_NAME
	);
	memcpy(nla + 1, "12", 2);
	jsonl_nl_sendto(fd, msg, msg_len);
	msg_open(fd);
	jsonl_array_open();
	printf("{");
	nla_hdr(nla, "UNIX_DIAG_NAME");
	printf("}, {");
	jsonl_bytes_value(nla + 1, 2, false);
	printf("}");
	jsonl_array_close();
	msg_close();

	/* one struct nlattr and short read of the second struct nlattr */
	msg_len = NLMSG_SPACE(sizeof(msg->udm)) + NLA_HDRLEN * 2;
	msg = tail_memdup(&c_msg, msg_len - 1);
	memcpy(&msg->nlh.nlmsg_len, &msg_len, sizeof(msg_len));
	nla = NLMSG_ATTR(msg, sizeof(msg->udm));
	SET_STRUCT(struct nlattr, nla,
		.nla_len = NLA_HDRLEN,
		.nla_type = UNIX_DIAG_NAME
	);
	jsonl_nl_sendto(fd, msg, msg_len);
	msg_open(fd);
	jsonl_array_open();
	printf("{");
	nla_hdr(nla, "UNIX_DIAG_NAME");
	printf("}");
	jsonl_array_fetch_failed(nla + 1);
	msg_close();

	/* two struct nlattr */
	msg_len = NLMSG_SPACE(sizeof(msg->udm)) + NLA_HDRLEN * 2;
	msg = tail_memdup(&c_msg, msg_len);
	memcpy(&msg->nlh.nlmsg_len, &msg_len, sizeof(msg_len));
	nla = NLMSG_ATTR(msg, sizeof(msg->udm));
	*nla = (struct nlattr) {
		.nla_len = NLA_HDRLEN,
		.nla_type = UNIX_DIAG_NAME
	};
	*(nla + 1) = (struct nlattr) {
		.nla_len = NLA_HDRLEN,
		.nla_type = UNIX_DIAG_PEER
	};
	jsonl_nl_sendto(fd, msg, msg_len);
	msg_open(fd);
	jsonl_array_open();
	printf("{");
	nla_hdr(nla, "UNIX_DIAG_NAME");
	printf("}, {");
	nla_hdr(nla + 1, "UNIX_DIAG_PEER");
	printf("}");
	jsonl_array_close();
	msg_close();

	/* the first nlattr only when its nla_len is less than NLA_HDRLEN */
	nla->nla_len = NLA_HDRLEN - 1;
	jsonl_nl_sendto(fd, msg, msg_len);
	msg_open(fd);
	nla_hdr(nla, "UNIX_DIAG_NAME");
	msg_close();

	/* unrecognized attribute data, abbreviated output */
# define ABBREV_LEN (DEFAULT_STRLEN + 1)
	msg_len = NLMSG_SPACE(sizeof(msg->udm)) + NLA_HDRLEN * ABBREV_LEN * 2;
	msg = tail_alloc(msg_len);
	memcpy(msg, &c_msg, sizeof(c_msg));
	msg->nlh.nlmsg_len = msg_len;
	nla = NLMSG_ATTR(msg, sizeof(msg->udm));
	for (unsigned int i = 0; i < ABBREV_LEN; ++i) {
		nla[i * 2] = (struct nlattr) {
			.nla_len = NLA_HDRLEN * 2 - 1,
			.nla_type = UNIX_DIAG_FIRST_UNUSED + i
		};
		fill_memory_ex(&nla[i * 2 + 1], NLA_HDRLEN,
			       '0' + i, '~' - '0' - i);
	}
	jsonl_nl_sendto(fd, msg, msg_len);
	msg_open(fd);
	jsonl_array_open();
	for (unsigned int i = 0; i < DEFAULT_STRLEN; ++i) {
		printf("%s{", i ? ", " : "");
		jsonl_nlattr_open(nla[i * 2].nla_len, nla[i * 2].nla_type,
				  NULL, "UNIX_DIAG_???");
		jsonl_bytes_value(&nla[i * 2 + 1], NLA_HDRLEN - 1, false);
		jsonl_nlattr_close();
		printf("}");
	}
	jsonl_array_close();
	jsonl_truncated();
	msg_close();
}

static void
test_nla_type(const int fd)
{
	struct msg *msg;
	struct nlattr *nla;
	unsigned int msg_len;

	msg_len = NLMSG_SPACE(sizeof(msg->udm)) + sizeof(*nla);
	msg = tail_memdup(&c_msg, msg_len);
	memcpy(&msg->nlh.nlmsg_len, &msg_len, sizeof(msg_len));
	nla = NLMSG_ATTR(msg, sizeof(msg->udm));
	*nla = (struct nlattr) {
		.nla_len = sizeof(*nla),
		.nla_type = NLA_F_NESTED | UNIX_DIAG_NAME
	};
	jsonl_nl_sendto(fd, msg, msg_len);
	msg_open(fd);
	nla_hdr(nla, "UNIX_DIAG_NAME");
	msg_close();

	nla->nla_type = NLA_F_NET_BYTEORDER | UNIX_DIAG_NAME;
	jsonl_nl_sendto(fd, msg, msg_len);
	msg_open(fd);
	nla_hdr(nla, "UNIX_DIAG_NAME");
	msg_close();

	nla->nla_type = NLA_F_NESTED | NLA_F_NET_BYTEORDER | UNIX_DIAG_NAME;
	jsonl_nl_sendto(fd, msg, msg_len);
	msg_open(fd);
	nla_hdr(nla, "UNIX_DIAG_NAME");
	msg_close();

	nla->nla_type = NLA_F_NESTED | UNIX_DIAG_FIRST_UNUSED;
	jsonl_nl_sendto(fd, msg, msg_len);
	msg_open(fd);
	nla_hdr(nla, NULL);
	msg_close();
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	const int fd = create_nl_socket(NETLINK_SOCK_DIAG);

	test_nlattr(fd);
	test_nla_type(fd);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
