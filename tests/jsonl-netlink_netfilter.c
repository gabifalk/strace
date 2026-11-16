/*
 * Check JSONL output of NETLINK_NETFILTER messages.
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
# include <arpa/inet.h>
# include <linux/netfilter/nfnetlink.h>
# include <linux/netfilter/nf_tables.h>

struct msg_type {
	unsigned int val;
	/* A reserved control message type. */
	const char *sym;
	/* NULL when the subsystem is unknown. */
	const char *subsys;
	/* NULL when the message type is unknown. */
	const char *msg;
	const char *msg_pattern;
};

static void
send_msg(int fd, void *nlh0, const struct msg_type *type,
	 unsigned int data_len, const void *src, unsigned int slen)
{
	jsonl_nl_send_nlmsg(fd, nlh0, type->val, NLM_F_REQUEST,
			    data_len, src, slen);
}

static void
jsonl_type(const struct msg_type *type)
{
	if (type->sym) {
		jsonl_const(type->val, true, type->sym, NULL);
		return;
	}

	jsonl_nfnl_msg_type(type->val, type->subsys,
			    type->msg, type->msg_pattern);
}

static void
nlmsghdr(const struct msg_type *type)
{
	static const struct strval32 flags[] = {
		{ ARG_STR(NLM_F_REQUEST) }
	};

	jsonl_nlmsghdr_type_open(jsonl_nl_len);
	jsonl_type(type);
	jsonl_nlmsghdr_type_close(NLM_F_REQUEST, flags, ARRAY_SIZE(flags),
				  0, 0);
}

/*
 * The event up to the fields of the first payload element;
 * a message without a payload consists of the header alone.
 */
static void
msg_open(int fd, const struct msg_type *type, bool payload)
{
	jsonl_nl_sendto_open(fd);
	if (payload) {
		jsonl_array_open();
		printf("{");
	}
	nlmsghdr(type);
	if (payload)
		printf("}, {");
}

static void
msg_close(bool payload)
{
	if (payload)
		jsonl_nlmsg_close();
	jsonl_nl_sendto_close();
}

static void
nfgenmsg(const struct nfgenmsg *msg)
{
	jsonl_struct_open();
	jsonl_field_const("nfgen_family", true, AF_UNIX, "AF_UNIX", NULL);
	jsonl_field_const("version", false, NFNETLINK_V0, "NFNETLINK_V0", NULL);
	jsonl_field_open("res_id", false);
	jsonl_unsigned(ntohs(msg->res_id));
	jsonl_byteorder_big(sizeof(msg->res_id));
	jsonl_field_close();
	jsonl_struct_close();
}

static void
test_nlmsg_type(const int fd)
{
	static const struct msg_type types[] = {
		{
			.val = NFNL_MSG_BATCH_BEGIN,
			.sym = "NFNL_MSG_BATCH_BEGIN"
		}, {
			.val = NFNL_SUBSYS_CTNETLINK << JSONL_NFNL_SUBSYS_SHIFT
			       | 0xff,
			.subsys = "NFNL_SUBSYS_CTNETLINK",
			.msg_pattern = "IPCTNL_MSG_CT_???"
		}, {
			.val = 0xffff
		}
	};
	TAIL_ALLOC_OBJECT_CONST_PTR(struct nlmsghdr, h);

	for (unsigned int i = 0; i < ARRAY_SIZE(types); ++i) {
		send_msg(fd, h, &types[i], 0, NULL, 0);
		msg_open(fd, &types[i], false);
		msg_close(false);
	}
}

static void
test_nlmsg_done(const int fd)
{
	static const struct msg_type done = {
		.val = NLMSG_DONE,
		.sym = "NLMSG_DONE"
	};
	const int num = 0xabcdefad;
	void *const nlh0 = midtail_alloc(NLMSG_HDRLEN, sizeof(num));

	send_msg(fd, nlh0, &done, sizeof(num), &num, sizeof(num));
	msg_open(fd, &done, true);
	jsonl_decimal(num);
	msg_close(true);
}

static void
test_nfgenmsg(const int fd)
{
	static const struct msg_type newtable = {
		.val = NFNL_SUBSYS_NFTABLES << JSONL_NFNL_SUBSYS_SHIFT
		       | NFT_MSG_NEWTABLE,
		.subsys = "NFNL_SUBSYS_NFTABLES",
		.msg = "NFT_MSG_NEWTABLE"
	};
	static const struct msg_type batch_begin = {
		.val = NFNL_MSG_BATCH_BEGIN,
		.sym = "NFNL_MSG_BATCH_BEGIN"
	};
	static const char extra[] = "1234";

	struct nfgenmsg msg = {
		.nfgen_family = AF_UNIX,
		.version = NFNETLINK_V0,
		.res_id = htons(0xabcd)
	};
	char str_buf[NLMSG_ALIGN(sizeof(msg)) + sizeof(extra) - 1];
	char pattern[sizeof(msg) - 1];

	void *const nlh0 = midtail_alloc(NLMSG_HDRLEN, sizeof(str_buf));

	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	/* len < sizeof(msg) */
	send_msg(fd, nlh0, &newtable,
		 sizeof(pattern), pattern, sizeof(pattern));
	msg_open(fd, &newtable, true);
	jsonl_bytes_value(pattern, sizeof(pattern), false);
	msg_close(true);

	/* short read of sizeof(msg) */
	send_msg(fd, nlh0, &newtable, sizeof(msg), pattern, sizeof(pattern));
	msg_open(fd, &newtable, true);
	jsonl_addr(jsonl_nl_data);
	msg_close(true);

	send_msg(fd, nlh0, &newtable, sizeof(msg), &msg, sizeof(msg));
	msg_open(fd, &newtable, true);
	nfgenmsg(&msg);
	msg_close(true);

	msg.res_id = htons(NFNL_SUBSYS_NFTABLES);
	send_msg(fd, nlh0, &batch_begin, sizeof(msg), &msg, sizeof(msg));
	msg_open(fd, &batch_begin, true);
	nfgenmsg(&msg);
	msg_close(true);

	msg.res_id = htons(0xabcd);
	memcpy(str_buf, &msg, sizeof(msg));
	memcpy(str_buf + NLMSG_ALIGN(sizeof(msg)), extra, sizeof(extra) - 1);

	send_msg(fd, nlh0, &batch_begin,
		 sizeof(str_buf), str_buf, sizeof(str_buf));
	msg_open(fd, &batch_begin, true);
	nfgenmsg(&msg);
	jsonl_nlmsg_next();
	jsonl_bytes_value(extra, sizeof(extra) - 1, false);
	msg_close(true);
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	int fd = create_nl_socket(NETLINK_NETFILTER);

	test_nlmsg_type(fd);
	test_nlmsg_done(fd);
	test_nfgenmsg(fd);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
