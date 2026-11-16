/*
 * Check JSONL output of NLMSGERR_ATTR_* netlink attributes of nlmsgerr.
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

# define NLMSGERR_ATTR_COOKIE 3

static const struct nlmsgerr err = {
	.error = -EACCES,
	.msg = {
		.nlmsg_len = NLMSG_HDRLEN + 4,
		.nlmsg_type = NLMSG_NOOP,
		.nlmsg_flags = NLM_F_REQUEST,
	}
};

static void
init_nlmsgerr(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = NLMSG_ERROR,
		.nlmsg_flags = NLM_F_REQUEST | NLM_F_CAPPED
	);

	memcpy(NLMSG_DATA(nlh), &err, sizeof(err));
}

/*
 * The netlink header and the element with the error message
 * up to the end of its struct nlmsgerr.
 */
static void
print_nlmsgerr(const unsigned int msg_len)
{
	static const struct strval32 nlmsg_flags[] = {
		{ ARG_STR(NLM_F_REQUEST) },
		{ ARG_STR(NLM_F_CAPPED) }
	};
	static const struct strval32 msg_flags[] = {
		{ ARG_STR(NLM_F_REQUEST) }
	};

	printf("{");
	jsonl_nlmsghdr_vals(msg_len, ARG_STR(NLMSG_ERROR), NULL,
			    NLM_F_REQUEST | NLM_F_CAPPED,
			    nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	jsonl_array_open();
	printf("{");
	jsonl_struct_open();
	jsonl_field_open("error", true);
	jsonl_errno(err.error);
	jsonl_field_close();
	jsonl_field_open("msg", false);
	jsonl_nlmsghdr(&err.msg, "NLMSG_NOOP", NULL,
		       msg_flags, ARRAY_SIZE(msg_flags));
	jsonl_field_close();
	jsonl_struct_close();
	printf("}");
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	static const uint8_t cookie[] = { 0xab, 0xfe };

	const int fd = create_nl_socket(NETLINK_SOCK_DIAG);
	const unsigned int hdrlen = sizeof(struct nlmsgerr);
	void *const nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
			NLA_HDRLEN + sizeof(cookie));

	/* The attribute belongs to the array that begins with nlmsgerr. */
	jsonl_nl_send_nlattr(fd, nlh0, hdrlen, init_nlmsgerr,
			     NLMSGERR_ATTR_COOKIE,
			     sizeof(cookie), cookie, sizeof(cookie), 0);
	jsonl_nl_sendto_open(fd);
	jsonl_array_open();
	print_nlmsgerr(jsonl_nl_len);
	printf(", {");
	jsonl_nlattr_open(NLA_HDRLEN + sizeof(cookie),
			  ARG_STR(NLMSGERR_ATTR_COOKIE), NULL);
	jsonl_array_open();
	for (size_t i = 0; i < ARRAY_SIZE(cookie); ++i) {
		printf("%s{", i ? ", " : "");
		jsonl_unsigned(cookie[i]);
		printf("}");
	}
	jsonl_array_close();
	jsonl_nlattr_close();
	printf("}");
	jsonl_array_close();
	printf("}");
	jsonl_array_close();
	jsonl_nl_sendto_close();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
