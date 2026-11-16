/*
 * Check JSONL output of TCA_ROOT_* netlink attributes of tcamsg.
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

# include <linux/pkt_cls.h>

# if !HAVE_DECL_TCA_ACT_PAD
enum { TCA_ACT_PAD = 5 };
# endif
# if !HAVE_DECL_TCA_ACT_COOKIE
enum { TCA_ACT_COOKIE = 6 };
# endif
# if !HAVE_DECL_TCA_ACT_FLAGS
enum { TCA_ACT_FLAGS = 7 };
# endif
# if !HAVE_DECL_TCA_ACT_HW_STATS
enum { TCA_ACT_HW_STATS = 8 };
# endif
# if !HAVE_DECL_TCA_ACT_USED_HW_STATS
enum { TCA_ACT_USED_HW_STATS = 9 };
# endif
# if !HAVE_DECL_TCA_ACT_IN_HW_COUNT
enum { TCA_ACT_IN_HW_COUNT = 10 };
# endif

static const unsigned int hdrlen = sizeof(struct tcamsg);

static void
init_tcamsg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = RTM_GETACTION,
		.nlmsg_flags = NLM_F_DUMP
	);

	struct tcamsg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct tcamsg, msg,
		.tca_family = AF_INET
	);
}

static void
print_tcamsg(const unsigned int msg_len)
{
	static const struct strval32 nlmsg_flags[] = {
		{ ARG_STR(NLM_F_DUMP) }
	};

	printf("{");
	jsonl_nlmsghdr_vals(msg_len, ARG_STR(RTM_GETACTION), NULL,
			    NLM_F_DUMP, nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	jsonl_struct_open();
	jsonl_field_open("tca_family", true);
	jsonl_const(AF_INET, true, "AF_INET", NULL);
	jsonl_field_close();
	jsonl_struct_close();
	printf("}");
}

static void
init_tcamsg_tab(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	init_tcamsg(nlh, msg_len);

	struct nlattr *nla = NLMSG_ATTR(nlh, hdrlen);
	SET_STRUCT(struct nlattr, nla,
		.nla_len = msg_len - NLMSG_SPACE(hdrlen),
		.nla_type = TCA_ROOT_TAB,
	);
}

static void
print_tcamsg_tab(const unsigned int msg_len)
{
	print_tcamsg(msg_len);
	jsonl_nlattr_nest_open(msg_len - NLMSG_SPACE(hdrlen),
			       ARG_STR(TCA_ROOT_TAB), NULL);
}

static uint16_t tab_idx;

static void
init_tcamsg_tab_item(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	init_tcamsg_tab(nlh, msg_len);

	struct nlattr *nla = NLMSG_ATTR(nlh, hdrlen + NLA_HDRLEN);
	SET_STRUCT(struct nlattr, nla,
		.nla_len = msg_len - NLMSG_SPACE(hdrlen) - NLA_HDRLEN,
		.nla_type = tab_idx,
	);
}

/* The type of an item of TCA_ROOT_TAB is its index, it has no name. */
static void
print_tcamsg_tab_item(const unsigned int msg_len)
{
	print_tcamsg_tab(msg_len);
	jsonl_nlattr_nest_open(msg_len - NLMSG_SPACE(hdrlen) - NLA_HDRLEN,
			       tab_idx, NULL, NULL);
}

/* The fields of a ticks value that is a number of milliseconds. */
static void
print_msecs(const uint64_t val)
{
	printf("\"type\": \"ticks\", \"raw\": \"%" PRIu64 "\", "
	       "\"hz\": \"1000\", \"precision\": \"3\"", val);
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen), 3 * NLA_HDRLEN + 21);

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	/* Invalid */
	static const unsigned int nla_invalid[] = { 6, 0xffff & NLA_TYPE_MASK };
	for (size_t i = 0; i < ARRAY_SIZE(nla_invalid); i++) {
		JSONL_NLATTR_(fd, nlh0, hdrlen, init_tcamsg, print_tcamsg,
			      nla_invalid[i], NULL, "TCA_ROOT_???",
			      21, pattern, 21,
			      jsonl_bytes_value(pattern, 21, false));
	}

	/* Default decoder */
	JSONL_NLATTR(fd, nlh0, hdrlen, init_tcamsg, print_tcamsg,
		     TCA_ROOT_UNSPEC, 17, pattern, 17,
		     jsonl_bytes_value(pattern, 17, false));

	/* TCA_ROOT_TAB: Invalid */
	JSONL_NLATTR(fd, nlh0, hdrlen, init_tcamsg, print_tcamsg,
		     TCA_ROOT_TAB, 3, pattern, 3,
		     jsonl_bytes_value(pattern, 3, false));

	/* TCA_ROOT_TAB: item: invalid */
	JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
			     init_tcamsg_tab, print_tcamsg_tab,
			     0, NULL, NULL, 3, pattern, 3, 1,
			     jsonl_bytes_value(pattern, 3, false));
	tab_idx++;

	/* TCA_ROOT_TAB: item: default decoder */
	static const struct strval32 tcaa_default[] = {
		{ ARG_STR(TCA_ACT_UNSPEC) },
		{ ARG_STR(TCA_ACT_OPTIONS) },
		{ ARG_STR(TCA_ACT_PAD) },
		{ ARG_STR(TCA_ACT_COOKIE) },
		{ 11, NULL },
	};
	for (size_t i = 0; i < ARRAY_SIZE(tcaa_default); i++) {
		JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
				     init_tcamsg_tab_item,
				     print_tcamsg_tab_item,
				     tcaa_default[i].val, tcaa_default[i].str,
				     "TCA_ACT_???",
				     17, pattern, 17, 2,
				     jsonl_bytes_value(pattern, 17, false));
		tab_idx++;
	}

	/* TCA_ROOT_TAB: item: TCA_ACT_KIND */
	JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
			     init_tcamsg_tab_item, print_tcamsg_tab_item,
			     TCA_ACT_KIND, "TCA_ACT_KIND", NULL,
			     21, pattern, 21, 2,
			     jsonl_string_value(pattern, 21, true));
	tab_idx++;

	static const char kind[] = "Hello\tthere";
	JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
			     init_tcamsg_tab_item, print_tcamsg_tab_item,
			     TCA_ACT_KIND, "TCA_ACT_KIND", NULL,
			     sizeof(kind), kind, sizeof(kind), 2,
			     jsonl_string_value(kind, sizeof(kind) - 1, false));
	tab_idx++;

	/* TCA_ROOT_TAB: item: TCA_ACT_INDEX */
	static const uint32_t idx = 0xdeadc0de;
	JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
			     init_tcamsg_tab_item, print_tcamsg_tab_item,
			     TCA_ACT_INDEX, "TCA_ACT_INDEX", NULL,
			     sizeof(idx), &idx, sizeof(idx), 2,
			     jsonl_unsigned(idx));
	tab_idx++;

	/* TCA_ROOT_TAB: item: TCA_ACT_FLAGS */
	static const struct strval32 act_flags[] = {
		{ 0x1, "TCA_ACT_FLAGS_NO_PERCPU_STATS" },
		{ 0x2, "TCA_ACT_FLAGS_SKIP_HW" },
		{ 0x4, "TCA_ACT_FLAGS_SKIP_SW" },
	};
	static const uint32_t flags = 0xfacebeff;
	JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
			     init_tcamsg_tab_item, print_tcamsg_tab_item,
			     TCA_ACT_FLAGS, "TCA_ACT_FLAGS", NULL,
			     sizeof(flags), &flags, sizeof(flags), 2,
			     jsonl_flags(flags, act_flags,
					 ARRAY_SIZE(act_flags),
					 "TCA_ACT_FLAGS_???"));
	tab_idx++;

	/* TCA_ROOT_TAB: item: TCA_ACT_HW_STATS, TCA_ACT_USED_HW_STATS */
	static const struct strval32 nla_hw_st[] = {
		{ ARG_STR(TCA_ACT_HW_STATS) },
		{ ARG_STR(TCA_ACT_USED_HW_STATS) },
	};
	static const struct strval32 hw_stats_flags[] = {
		{ 0x1, "TCA_ACT_HW_STATS_IMMEDIATE" },
		{ 0x2, "TCA_ACT_HW_STATS_DELAYED" },
	};
	static const uint32_t hw_st = 0xfacebeef;
	for (size_t i = 0; i < ARRAY_SIZE(nla_hw_st); i++) {
		JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
				     init_tcamsg_tab_item,
				     print_tcamsg_tab_item,
				     nla_hw_st[i].val, nla_hw_st[i].str, NULL,
				     sizeof(hw_st), &hw_st, sizeof(hw_st), 2,
				     jsonl_flags(hw_st, hw_stats_flags,
						 ARRAY_SIZE(hw_stats_flags),
						 "TCA_ACT_HW_STATS_???"));
		tab_idx++;
	}

	/* TCA_ROOT_TAB: item: TCA_ACT_IN_HW_COUNT */
	static const uint32_t hw_count = 0xdeadface;
	JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,
			     init_tcamsg_tab_item, print_tcamsg_tab_item,
			     TCA_ACT_IN_HW_COUNT, "TCA_ACT_IN_HW_COUNT", NULL,
			     sizeof(hw_count), &hw_count, sizeof(hw_count), 2,
			     jsonl_unsigned(hw_count));

	/* TCA_ROOT_FLAGS */
	static const struct strval32 act_flag[] = {
		{ 0x1, "TCA_ACT_FLAG_LARGE_DUMP_ON" },
		{ 0x2, "TCA_ACT_FLAG_TERSE_DUMP" },
	};
	static const uint32_t root_flags[] = {
		0, 0x1, 0x3, 0xcafebeef, 0xbadc0dec
	};

	for (size_t i = 0; i < ARRAY_SIZE(root_flags); i++) {
		JSONL_NLATTR(fd, nlh0, hdrlen, init_tcamsg, print_tcamsg,
			     TCA_ROOT_FLAGS, 4, &root_flags[i], 4,
			     jsonl_flags(root_flags[i], act_flag,
					 ARRAY_SIZE(act_flag),
					 "TCA_ACT_FLAG_???"));
	}

	/* TCA_ROOT_COUNT */
	static const uint32_t cnt_vals[] = { 0, 1, 0xbac0ded };

	for (size_t i = 0; i < ARRAY_SIZE(cnt_vals); i++) {
		JSONL_NLATTR(fd, nlh0, hdrlen, init_tcamsg, print_tcamsg,
			     TCA_ROOT_COUNT, 4, &cnt_vals[i], 4,
			     jsonl_unsigned(cnt_vals[i]));
	}

	/* TCA_ROOT_TIME_DELTA */
	static const uint32_t time_deltas[] = {
		0, 1, 10, 100, 999, 1000, 1001, 1010, 0xfeedface
	};

	for (size_t i = 0; i < ARRAY_SIZE(time_deltas); i++) {
		JSONL_NLATTR(fd, nlh0, hdrlen, init_tcamsg, print_tcamsg,
			     TCA_ROOT_TIME_DELTA, 4, &time_deltas[i], 4,
			     print_msecs(time_deltas[i]));
	}

	static const uint64_t time_deltas64[] = {
		0, 1, 10, 100, 999, 1000, 1001, 1010, 0xfeedface,
		0xbadfacedeadc0ded
	};

	for (size_t i = 0; i < ARRAY_SIZE(time_deltas64); i++) {
		JSONL_NLATTR(fd, nlh0, hdrlen, init_tcamsg, print_tcamsg,
			     TCA_ROOT_TIME_DELTA, 8, &time_deltas64[i], 8,
			     print_msecs(time_deltas64[i]));
	}

	/* TCA_ROOT_EXT_WARN_MSG */
	static const char msg[] = "Hello\tthere";
	JSONL_NLATTR(fd, nlh0, hdrlen, init_tcamsg, print_tcamsg,
		     TCA_ROOT_EXT_WARN_MSG, sizeof(msg), msg, sizeof(msg),
		     jsonl_string_value(msg, sizeof(msg) - 1, false));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
