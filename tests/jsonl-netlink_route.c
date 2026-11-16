/*
 * Check JSONL output of NETLINK_ROUTE messages.
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

# include <linux/if_addr.h>
# include <linux/if_arp.h>
# include <linux/if_link.h>
# include <linux/ip.h>

static const struct strval32 nlm_f_request[] = {
	{ ARG_STR(NLM_F_REQUEST) }
};

/* A struct field that follows another field and holds a single flag. */
static void
flag_field(const char *name, uint32_t val, const char *sym)
{
	const struct strval32 flag = { val, sym };

	jsonl_field_open(name, false);
	jsonl_flags(val, &flag, 1, NULL);
	jsonl_field_close();
}

static void
const_field(const char *name, uint32_t val, const char *sym)
{
	jsonl_field_open(name, false);
	jsonl_const(val, true, sym, NULL);
	jsonl_field_close();
}

static void
u_field(const char *name, unsigned int val)
{
	jsonl_field_open(name, false);
	jsonl_unsigned(val);
	jsonl_field_close();
}

static void
ifindex_lo_field(const char *name)
{
	jsonl_field_open(name, false);
	jsonl_ifindex(ifindex_lo(), "lo");
	jsonl_field_close();
}

/*
 * A struct value up to the end of its first field, the address family;
 * sym is NULL when the address family is unknown.
 */
static void
family_open(const char *name, unsigned int family, const char *sym)
{
	jsonl_struct_open();
	jsonl_field_open(name, true);
	jsonl_const(family, true, sym, "AF_???");
	jsonl_field_close();
}

/* The variable arguments print the fields of the payload value. */
# define TEST_NETLINK_(fd_, nlh0_, type_, type_sym_,			\
		       data_len_, src_, slen_, ...)			\
	JSONL_NETLINK_((fd_), (nlh0_), (type_), (type_sym_), "RTM_???",	\
		       NLM_F_REQUEST, nlm_f_request,			\
		       ARRAY_SIZE(nlm_f_request),			\
		       (data_len_), (src_), (slen_), __VA_ARGS__)

/*
 * A message that is one byte too short for the object, a message with
 * the whole object, and a message whose object is not readable past
 * its first byte, the address family.  The variable arguments print
 * the fields that follow the address family.
 */
# define TEST_NL_ROUTE_(fd_, nlh0_, type_, type_sym_, obj_,		\
			family_name_, ...)				\
	do {								\
		TEST_NETLINK_((fd_), (nlh0_), (type_), (type_sym_),	\
			      sizeof(obj_) - 1,				\
			      &(obj_), sizeof(obj_) - 1,		\
			      family_open((family_name_),		\
					  AF_UNIX, "AF_UNIX");		\
			      jsonl_struct_close();			\
			      jsonl_truncated());			\
		TEST_NETLINK_((fd_), (nlh0_), (type_), (type_sym_),	\
			      sizeof(obj_), &(obj_), sizeof(obj_),	\
			      family_open((family_name_),		\
					  AF_UNIX, "AF_UNIX");		\
			      __VA_ARGS__;				\
			      jsonl_struct_close());			\
		TEST_NETLINK_((fd_), (nlh0_), (type_), (type_sym_),	\
			      sizeof(obj_), &(obj_), sizeof(obj_) - 1,	\
			      family_open((family_name_),		\
					  AF_UNIX, "AF_UNIX");		\
			      jsonl_struct_close_fetch_failed(		\
					jsonl_nl_data + 1));		\
	} while (0)

/* A message that consists of the header alone is not an array. */
static void
test_nlmsghdr(int fd, unsigned int type, const char *type_sym,
	      const struct strval32 *flags, size_t nflags)
{
	struct nlmsghdr nlh = {
		.nlmsg_len = sizeof(nlh),
		.nlmsg_type = type,
	};

	for (size_t i = 0; i < nflags; ++i)
		nlh.nlmsg_flags |= flags[i].val;

	jsonl_nl_sendto(fd, &nlh, sizeof(nlh));
	jsonl_nl_sendto_open(fd);
	jsonl_nlmsghdr(&nlh, type_sym, "RTM_???", flags, nflags);
	jsonl_nl_sendto_close();
}

static void
test_nlmsg_type(const int fd)
{
	test_nlmsghdr(fd, ARG_STR(RTM_GETLINK),
		      nlm_f_request, ARRAY_SIZE(nlm_f_request));
}

/* The names of the flags above 0xff depend on the message type. */
static void
test_nlmsg_flags(const int fd)
{
	static const struct strval32 get_flags[] = {
		{ ARG_STR(NLM_F_REQUEST) },
		{ ARG_STR(NLM_F_DUMP) },
	};
	static const struct strval32 act_flags[] = {
		{ ARG_STR(NLM_F_ROOT) },
	};
	static const struct strval32 new_flags[] = {
		{ ARG_STR(NLM_F_ECHO) },
		{ ARG_STR(NLM_F_REPLACE) },
	};
	static const struct strval32 del_flags[] = {
		{ ARG_STR(NLM_F_NONREC) },
		{ ARG_STR(NLM_F_BULK) },
	};

	test_nlmsghdr(fd, ARG_STR(RTM_GETLINK),
		      get_flags, ARRAY_SIZE(get_flags));
	test_nlmsghdr(fd, ARG_STR(RTM_DELACTION),
		      act_flags, ARRAY_SIZE(act_flags));
	test_nlmsghdr(fd, ARG_STR(RTM_NEWLINK),
		      new_flags, ARRAY_SIZE(new_flags));
	test_nlmsghdr(fd, ARG_STR(RTM_DELLINK),
		      del_flags, ARRAY_SIZE(del_flags));
}

static void
test_nlmsg_done(const int fd, void *const nlh0)
{
	const int num = 0xabcdefad;

	JSONL_NETLINK(fd, nlh0, NLMSG_DONE, NLM_F_REQUEST,
		      sizeof(num), &num, sizeof(num),
		      jsonl_decimal(num));

	/* A payload shorter than an integer is not decoded. */
	JSONL_NETLINK(fd, nlh0, NLMSG_DONE, NLM_F_REQUEST,
		      sizeof(num) - 1, &num, sizeof(num) - 1,
		      jsonl_bytes_value(&num, sizeof(num) - 1, false));
}

static void
test_rtnl_unspec(const int fd, void *const nlh0)
{
	uint8_t family = 0;
	char buf[sizeof(family) + 4];

	/* unspecified family only */
	TEST_NETLINK_(fd, nlh0, 0xffff, NULL,
		      sizeof(family), &family, sizeof(family),
		      family_open("family", family, "AF_UNSPEC");
		      jsonl_struct_close());

	/* unknown family only */
	family = 0xff;
	TEST_NETLINK_(fd, nlh0, 0xffff, NULL,
		      sizeof(family), &family, sizeof(family),
		      family_open("family", family, NULL);
		      jsonl_struct_close());

	/* short read of family */
	TEST_NETLINK_(fd, nlh0, 0xffff, NULL,
		      sizeof(family), &family, sizeof(family) - 1,
		      jsonl_addr(jsonl_nl_data));

	/* unspecified family and string */
	family = 0;
	memcpy(buf, &family, sizeof(family));
	memcpy(buf + sizeof(family), "1234", 4);
	TEST_NETLINK_(fd, nlh0, 0xffff, NULL,
		      sizeof(buf), buf, sizeof(buf),
		      family_open("family", family, "AF_UNSPEC");
		      jsonl_field_open("data", false);
		      jsonl_bytes_value(buf + sizeof(family), 4, false);
		      jsonl_field_close();
		      jsonl_struct_close());

	/* unknown family and string */
	family = 0xfd;
	memcpy(buf, &family, sizeof(family));
	TEST_NETLINK_(fd, nlh0, 0xffff, NULL,
		      sizeof(buf), buf, sizeof(buf),
		      family_open("family", family, NULL);
		      jsonl_field_open("data", false);
		      jsonl_bytes_value(buf + sizeof(family), 4, false);
		      jsonl_field_close();
		      jsonl_struct_close());
}

/* A message type that has no decoder: the address family and the data. */
static void
test_rtnl_unknown_msg(const int fd, void *const nlh0, uint16_t msg)
{
	char buf[64];

	fill_memory(buf, sizeof(buf));
	buf[0] = AF_INET;

	TEST_NETLINK_(fd, nlh0, msg, NULL, 1, buf, 1,
		      family_open("family", AF_INET, "AF_INET");
		      jsonl_struct_close());

	TEST_NETLINK_(fd, nlh0, msg, NULL, sizeof(buf), buf, sizeof(buf),
		      family_open("family", AF_INET, "AF_INET");
		      jsonl_field_open("data", false);
		      jsonl_bytes_value(buf + 1, DEFAULT_STRLEN, true);
		      jsonl_field_close();
		      jsonl_struct_close());
}

static void
test_rtnl_link(const int fd, void *const nlh0)
{
	static const struct strval32 types[] = {
		{ ARG_STR(RTM_NEWLINK) },
		{ ARG_STR(RTM_DELLINK) },
		{ ARG_STR(RTM_GETLINK) },
		{ ARG_STR(RTM_SETLINK) },
	};
	const struct ifinfomsg ifinfo = {
		.ifi_family = AF_UNIX,
		.ifi_type = ARPHRD_LOOPBACK,
		.ifi_index = ifindex_lo(),
		.ifi_flags = IFF_UP,
		.ifi_change = 0xfabcdeba
	};

	for (size_t i = 0; i < ARRAY_SIZE(types); i++) {
		TEST_NL_ROUTE_(fd, nlh0, types[i].val, types[i].str, ifinfo,
			       "ifi_family",
			       const_field("ifi_type",
					   ARG_STR(ARPHRD_LOOPBACK));
			       ifindex_lo_field("ifi_index");
			       flag_field("ifi_flags", ARG_STR(IFF_UP));
			       jsonl_field_open("ifi_change", false);
			       jsonl_hex(ifinfo.ifi_change);
			       jsonl_field_close());
	}
}

static void
test_rtnl_addr(const int fd, void *const nlh0)
{
	static const struct strval32 types[] = {
		{ ARG_STR(RTM_NEWADDR) },
		{ ARG_STR(RTM_DELADDR) },
		{ ARG_STR(RTM_GETADDR) },
		{ ARG_STR(RTM_NEWMULTICAST) },
		{ ARG_STR(RTM_DELMULTICAST) },
		{ ARG_STR(RTM_GETMULTICAST) },
		{ ARG_STR(RTM_NEWANYCAST) },
		{ ARG_STR(RTM_DELANYCAST) },
		{ ARG_STR(RTM_GETANYCAST) },
	};
	const struct ifaddrmsg msg = {
		.ifa_family = AF_UNIX,
		.ifa_prefixlen = 0xde,
		.ifa_flags = IFA_F_SECONDARY,
		.ifa_scope = RT_SCOPE_UNIVERSE,
		.ifa_index = ifindex_lo()
	};

	for (size_t i = 0; i < ARRAY_SIZE(types); i++) {
		TEST_NL_ROUTE_(fd, nlh0, types[i].val, types[i].str, msg,
			       "ifa_family",
			       u_field("ifa_prefixlen", msg.ifa_prefixlen);
			       flag_field("ifa_flags",
					  ARG_STR(IFA_F_SECONDARY));
			       const_field("ifa_scope",
					   ARG_STR(RT_SCOPE_UNIVERSE));
			       ifindex_lo_field("ifa_index"));
	}

	test_rtnl_unknown_msg(fd, nlh0, RTM_NEWADDR + 3);
	test_rtnl_unknown_msg(fd, nlh0, RTM_GETMULTICAST + 1);
	test_rtnl_unknown_msg(fd, nlh0, RTM_GETANYCAST + 1);
}

static void
test_rtnl_route(const int fd, void *const nlh0)
{
	static const struct strval32 types[] = {
		{ ARG_STR(RTM_NEWROUTE) },
		{ ARG_STR(RTM_DELROUTE) },
		{ ARG_STR(RTM_GETROUTE) },
	};
	static const struct rtmsg msg = {
		.rtm_family = AF_UNIX,
		.rtm_dst_len = 0xaf,
		.rtm_src_len = 0xda,
		.rtm_tos = IPTOS_LOWDELAY,
		.rtm_table = RT_TABLE_DEFAULT,
		.rtm_protocol = RTPROT_KERNEL,
		.rtm_scope = RT_SCOPE_UNIVERSE,
		.rtm_type = RTN_LOCAL,
		.rtm_flags = RTM_F_NOTIFY
	};

	for (size_t i = 0; i < ARRAY_SIZE(types); i++) {
		TEST_NL_ROUTE_(fd, nlh0, types[i].val, types[i].str, msg,
			       "rtm_family",
			       u_field("rtm_dst_len", msg.rtm_dst_len);
			       u_field("rtm_src_len", msg.rtm_src_len);
			       flag_field("rtm_tos", ARG_STR(IPTOS_LOWDELAY));
			       const_field("rtm_table",
					   ARG_STR(RT_TABLE_DEFAULT));
			       const_field("rtm_protocol",
					   ARG_STR(RTPROT_KERNEL));
			       const_field("rtm_scope",
					   ARG_STR(RT_SCOPE_UNIVERSE));
			       const_field("rtm_type", ARG_STR(RTN_LOCAL));
			       flag_field("rtm_flags", ARG_STR(RTM_F_NOTIFY)));
	}

	test_rtnl_unknown_msg(fd, nlh0, RTM_NEWROUTE + 3);
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *const nlh0 = midtail_alloc(NLMSG_HDRLEN, 64);

	test_nlmsg_type(fd);
	test_nlmsg_flags(fd);
	test_nlmsg_done(fd, nlh0);
	test_rtnl_unspec(fd, nlh0);

	test_rtnl_link(fd, nlh0);
	test_rtnl_addr(fd, nlh0);
	test_rtnl_route(fd, nlh0);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
