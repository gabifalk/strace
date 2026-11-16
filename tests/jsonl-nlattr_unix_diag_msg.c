/*
 * Check JSONL output of UNIX_DIAG_* netlink attributes of unix_diag_msg.
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

static void
print_u_field(const char *name, uint64_t val, bool first)
{
	jsonl_field_open(name, first);
	jsonl_unsigned(val);
	jsonl_field_close();
}

static void
init_unix_diag_msg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = SOCK_DIAG_BY_FAMILY,
		.nlmsg_flags = NLM_F_DUMP
	);

	struct unix_diag_msg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct unix_diag_msg, msg,
		.udiag_family = AF_UNIX,
		.udiag_type = SOCK_STREAM,
		.udiag_state = TCP_ESTABLISHED
	);
}

static void
print_unix_diag_msg(const unsigned int msg_len)
{
	static const struct strval32 nlmsg_flags[] = {
		{ ARG_STR(NLM_F_DUMP) }
	};

	printf("{");
	jsonl_nlmsghdr_vals(msg_len, ARG_STR(SOCK_DIAG_BY_FAMILY), NULL,
			    NLM_F_DUMP, nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	jsonl_struct_open();
	jsonl_field_open("udiag_family", true);
	jsonl_const(AF_UNIX, true, "AF_UNIX", NULL);
	jsonl_field_close();
	jsonl_field_open("udiag_type", false);
	jsonl_const(SOCK_STREAM, true, "SOCK_STREAM", NULL);
	jsonl_field_close();
	jsonl_field_open("udiag_state", false);
	jsonl_const(TCP_ESTABLISHED, true, "TCP_ESTABLISHED", NULL);
	jsonl_field_close();
	print_u_field("udiag_ino", 0, false);
	jsonl_field_open("udiag_cookie", false);
	jsonl_array_open();
	printf("{");
	jsonl_unsigned(0);
	printf("}, {");
	jsonl_unsigned(0);
	printf("}");
	jsonl_array_close();
	jsonl_field_close();
	jsonl_struct_close();
	printf("}");
}

static void
print_uint(const uint32_t *p, size_t i)
{
	jsonl_unsigned(*p);
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	static const struct unix_diag_vfs uv = {
		.udiag_vfs_dev = 0xabcddafa,
		.udiag_vfs_ino = 0xbafabcda
	};
	static const struct unix_diag_rqlen rql = {
		.udiag_rqueue = 0xfabdcdad,
		.udiag_wqueue = 0xbacdadcf
	};
	static const uint32_t inode[] = { 0xadbcadbc, 0xfabdcdac };

	const int fd = create_nl_socket(NETLINK_SOCK_DIAG);
	const unsigned int hdrlen = sizeof(struct unix_diag_msg);
	void *const nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
					 NLA_HDRLEN + sizeof(inode));

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen,
			    init_unix_diag_msg, print_unix_diag_msg,
			    UNIX_DIAG_VFS, pattern, uv,
			    jsonl_struct_open();
			    jsonl_field_open("udiag_vfs_dev", true);
			    jsonl_dev_t(uv.udiag_vfs_dev);
			    jsonl_field_close();
			    print_u_field("udiag_vfs_ino", uv.udiag_vfs_ino,
					  false);
			    jsonl_struct_close());

	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen,
			    init_unix_diag_msg, print_unix_diag_msg,
			    UNIX_DIAG_RQLEN, pattern, rql,
			    jsonl_struct_open();
			    print_u_field("udiag_rqueue", rql.udiag_rqueue,
					  true);
			    print_u_field("udiag_wqueue", rql.udiag_wqueue,
					  false);
			    jsonl_struct_close());

	JSONL_NLATTR_ARRAY(fd, nlh0, hdrlen,
			   init_unix_diag_msg, print_unix_diag_msg,
			   UNIX_DIAG_ICONS, pattern, inode, print_uint);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
