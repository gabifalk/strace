/*
 * Check JSONL output of NETLINK_GENERIC messages.
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

# include <linux/genetlink.h>

static const struct strval32 nlmsg_flags[] = {
	{ ARG_STR(NLM_F_REQUEST) },
	{ ARG_STR(NLM_F_DUMP) }
};

static void
print_genlmsghdr_fields(const struct genlmsghdr *const hdr)
{
	jsonl_struct_open();
	jsonl_field_hex("cmd", true, hdr->cmd);
	jsonl_field_unsigned("version", false, hdr->version);
	if (hdr->reserved) {
		jsonl_field_hex("reserved", false, hdr->reserved);
	}
	jsonl_struct_close();
}

static void
test_hdr(const int fd)
{
	static const struct genlmsghdr hdr = {
		.cmd = CTRL_CMD_GETFAMILY,
		.version = 1
	};
	void *const nlh0 = midtail_alloc(NLMSG_HDRLEN, sizeof(hdr));

	JSONL_NETLINK_OBJECT_EX_(fd, nlh0,
				 0xffff, NULL, "GENERIC_FAMILY_???",
				 NLM_F_REQUEST | NLM_F_DUMP,
				 nlmsg_flags, ARRAY_SIZE(nlmsg_flags),
				 hdr, jsonl_nl_bytes,
				 print_genlmsghdr_fields(&hdr));
}

static const struct genlmsghdr nla_hdr = {
	.cmd = CTRL_CMD_GETFAMILY,
	.version = 2,
	.reserved = 0xfeed
};

static void
init_genlmsghdr(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = 0xffff,
		.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP
	);

	memcpy(NLMSG_DATA(nlh), &nla_hdr, sizeof(nla_hdr));
}

static void
print_genlmsghdr(const unsigned int msg_len)
{
	printf("{");
	jsonl_nlmsghdr_vals(msg_len, 0xffff, NULL, "GENERIC_FAMILY_???",
			    NLM_F_REQUEST | NLM_F_DUMP,
			    nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	print_genlmsghdr_fields(&nla_hdr);
	printf("}");
}

static void
test_nla(const int fd)
{
	static char pattern[DEFAULT_STRLEN];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	const unsigned int hdrlen = sizeof(struct genlmsghdr);
	void *const nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
					 NLA_HDRLEN + sizeof(pattern));

	/* Attributes of an unknown family have no table of types. */
	JSONL_NLATTR_(fd, nlh0, hdrlen,
		      init_genlmsghdr, print_genlmsghdr,
		      CTRL_ATTR_OP, NULL, NULL,
		      sizeof(pattern), pattern, sizeof(pattern),
		      jsonl_bytes_value(pattern, sizeof(pattern), false));
}

static void
test_nlmsg_done(const int fd)
{
	const int num = 0xabcdefad;
	void *const nlh0 = midtail_alloc(NLMSG_HDRLEN, sizeof(num));

	JSONL_NETLINK(fd, nlh0, NLMSG_DONE, NLM_F_REQUEST,
		      sizeof(num), &num, sizeof(num),
		      jsonl_decimal(num));
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	int fd = create_nl_socket(NETLINK_GENERIC);

	test_hdr(fd);
	test_nla(fd);
	test_nlmsg_done(fd);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
