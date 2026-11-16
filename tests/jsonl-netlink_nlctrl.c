/*
 * Check JSONL output of NETLINK_GENERIC nlctrl messages.
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
# include <linux/cgroupstats.h>
# include <linux/devlink.h>
# include <linux/ethtool_netlink.h>
# include <linux/ioam6_genl.h>
# include <linux/mptcp_pm.h>
# include <linux/netdev.h>
# include <linux/nl80211.h>
# include <linux/seg6_genl.h>
# include <linux/tcp_metrics.h>
# include <linux/thermal.h>

static const unsigned int hdrlen = sizeof(struct genlmsghdr);

static const struct strval32 nlmsg_flags[] = {
	{ ARG_STR(NLM_F_REQUEST) },
	{ ARG_STR(NLM_F_DUMP) }
};

static const struct strval32 genl_flags[] = {
	{ ARG_STR(GENL_ADMIN_PERM) },
	{ ARG_STR(GENL_CMD_CAP_DO) },
	{ ARG_STR(GENL_CMD_CAP_DUMP) },
	{ ARG_STR(GENL_CMD_CAP_HASPOL) },
	{ ARG_STR(GENL_UNS_ADMIN_PERM) }
};

static void
print_genlmsghdr_fields(const struct genlmsghdr *const hdr,
			const char *const cmd)
{
	jsonl_struct_open();
	jsonl_field_open("cmd", true);
	jsonl_const(hdr->cmd, true, cmd, NULL);
	jsonl_field_close();
	jsonl_field_open("version", false);
	jsonl_unsigned(hdr->version);
	jsonl_field_close();
	if (hdr->reserved) {
		jsonl_field_open("reserved", false);
		jsonl_hex(hdr->reserved);
		jsonl_field_close();
	}
	jsonl_struct_close();
}

static void
test_hdr(const int fd)
{
	static const struct strval32 cmds[] = {
		{ ARG_STR(CTRL_CMD_UNSPEC) },
		{ ARG_STR(CTRL_CMD_NEWFAMILY) },
		{ ARG_STR(CTRL_CMD_DELFAMILY) },
		{ ARG_STR(CTRL_CMD_GETFAMILY) },
		{ ARG_STR(CTRL_CMD_NEWOPS) },
		{ ARG_STR(CTRL_CMD_DELOPS) },
		{ ARG_STR(CTRL_CMD_GETOPS) },
		{ ARG_STR(CTRL_CMD_NEWMCAST_GRP) },
		{ ARG_STR(CTRL_CMD_DELMCAST_GRP) },
		{ ARG_STR(CTRL_CMD_GETMCAST_GRP) },
		{ ARG_STR(CTRL_CMD_GETPOLICY) },
	};
	struct genlmsghdr hdr = {
		.version = 1
	};
	void *const nlh0 = midtail_alloc(NLMSG_HDRLEN, sizeof(hdr));

	for (size_t i = 0; i < ARRAY_SIZE(cmds); ++i) {
		hdr.cmd = cmds[i].val;
		JSONL_NETLINK_OBJECT_EX_(fd, nlh0,
					 GENL_ID_CTRL, "nlctrl", NULL,
					 NLM_F_REQUEST | NLM_F_DUMP,
					 nlmsg_flags, ARRAY_SIZE(nlmsg_flags),
					 hdr, jsonl_nl_bytes,
					 print_genlmsghdr_fields(&hdr,
								 cmds[i].str));
	}
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
		.nlmsg_type = GENL_ID_CTRL,
		.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP
	);

	memcpy(NLMSG_DATA(nlh), &nla_hdr, sizeof(nla_hdr));
}

static void
print_genlmsghdr(const unsigned int msg_len)
{
	printf("{");
	jsonl_nlmsghdr_vals(msg_len, GENL_ID_CTRL, "nlctrl", NULL,
			    NLM_F_REQUEST | NLM_F_DUMP,
			    nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	print_genlmsghdr_fields(&nla_hdr, "CTRL_CMD_GETFAMILY");
	printf("}");
}

/* An array element with an attribute, up to the fields of its payload. */
static void
attr_open(const bool first, const struct nlattr *const nla,
	  const char *const sym)
{
	printf("%s{", first ? "" : ", ");
	jsonl_nlattr_open(nla->nla_len, nla->nla_type, sym, NULL);
}

static void
attr_close(void)
{
	jsonl_nlattr_close();
	printf("}");
}

static void
test_nla_unknown(const int fd, void *const nlh0)
{
	static char pattern[DEFAULT_STRLEN];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	JSONL_NLATTR_(fd, nlh0, hdrlen,
		      init_genlmsghdr, print_genlmsghdr,
		      CTRL_ATTR_OP + 1, NULL, "CTRL_ATTR_???",
		      sizeof(pattern), pattern, sizeof(pattern),
		      jsonl_bytes_value(pattern, sizeof(pattern), false));
}

static void
test_nla_x16(const int fd, void *const nlh0)
{
	char pattern[sizeof(uint16_t)];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	JSONL_NLATTR_VAL(x16, fd, nlh0, hdrlen,
			 init_genlmsghdr, print_genlmsghdr,
			 CTRL_ATTR_FAMILY_ID, pattern, 0);
}

static void
test_nla_u32(const int fd, void *const nlh0)
{
	static const struct strval32 attrs[] = {
		{ ARG_STR(CTRL_ATTR_VERSION) },
		{ ARG_STR(CTRL_ATTR_HDRSIZE) },
		{ ARG_STR(CTRL_ATTR_MAXATTR) },
		{ ARG_STR(CTRL_ATTR_OP) },
	};
	char pattern[sizeof(uint32_t)];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	for (size_t i = 0; i < ARRAY_SIZE(attrs); ++i) {
		jsonl_check_u32_nlattr(fd, nlh0, hdrlen,
				       init_genlmsghdr, print_genlmsghdr,
				       attrs[i].val, attrs[i].str, NULL,
				       pattern, 0);
	}
}

static void
test_nla_str(const int fd, void *const nlh0)
{
	char str[DEFAULT_STRLEN];

	fill_memory_ex(str, sizeof(str), '0', 10);
	JSONL_NLATTR(fd, nlh0, hdrlen,
		     init_genlmsghdr, print_genlmsghdr,
		     CTRL_ATTR_FAMILY_NAME,
		     sizeof(str), str, sizeof(str),
		     jsonl_string_value(str, sizeof(str), true));

	str[sizeof(str) - 1] = '\0';
	JSONL_NLATTR(fd, nlh0, hdrlen,
		     init_genlmsghdr, print_genlmsghdr,
		     CTRL_ATTR_FAMILY_NAME,
		     sizeof(str), str, sizeof(str),
		     jsonl_string_value(str, sizeof(str) - 1, false));
}

struct u32_attr {
	struct nlattr h;
	uint32_t v;
};

struct op {
	struct nlattr h;
	struct u32_attr id;
	struct u32_attr flags;
};

/* The fields of the payload of CTRL_ATTR_OPS. */
static void
print_ops(const struct op *const ops, const size_t n,
	  const struct strval32 *const cmds, const char *const cmd_pattern)
{
	jsonl_array_open();
	for (size_t i = 0; i < n; ++i) {
		attr_open(!i, &ops[i].h, NULL);
		jsonl_array_open();
		attr_open(true, &ops[i].id.h, "CTRL_ATTR_OP_ID");
		jsonl_const(ops[i].id.v, true, cmds[i].str, cmd_pattern);
		attr_close();
		attr_open(false, &ops[i].flags.h, "CTRL_ATTR_OP_FLAGS");
		jsonl_flags(ops[i].flags.v, genl_flags, ARRAY_SIZE(genl_flags),
			    "GENL_???");
		attr_close();
		jsonl_array_close();
		attr_close();
	}
	jsonl_array_close();
}

static void
init_op(struct op *const op, const unsigned int idx, const uint32_t cmd,
	const uint32_t flags)
{
	*op = (struct op) {
		{ sizeof(*op), idx },
		{ { sizeof(op->id), CTRL_ATTR_OP_ID }, cmd },
		{ { sizeof(op->flags), CTRL_ATTR_OP_FLAGS }, flags }
	};
}

static void
test_nla_ops(const int fd)
{
	static const struct strval32 cmds[] = {
		{ ARG_STR(CTRL_CMD_GETFAMILY) },
		{ ARG_STR(CTRL_CMD_GETPOLICY) },
	};
	struct op src[ARRAY_SIZE(cmds)];

	init_op(&src[0], 1, cmds[0].val,
		GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL);
	init_op(&src[1], 2, cmds[1].val,
		GENL_ADMIN_PERM | GENL_UNS_ADMIN_PERM);

	void *const nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
					 NLA_HDRLEN + sizeof(src));

	JSONL_NLATTR(fd, nlh0, hdrlen,
		     init_genlmsghdr, print_genlmsghdr, CTRL_ATTR_OPS,
		     sizeof(src), src, sizeof(src),
		     print_ops(src, ARRAY_SIZE(src), cmds, NULL));
}

static void
test_nla_mcast(const int fd)
{
	struct {
		struct nlattr h;
		struct u32_attr a0;
		struct {
			struct nlattr h;
			char s[DEFAULT_STRLEN];
		} a1;
	} src[] = {
		{
			{ sizeof(src[0]), 1 },
			{
				{ sizeof(src[0].a0), CTRL_ATTR_MCAST_GRP_ID },
				0xcafef00d
			}, {
				{ sizeof(src[0].a1), CTRL_ATTR_MCAST_GRP_NAME },
				""
			}
		}, {
			{ sizeof(src[1]), 2 },
			{
				{ sizeof(src[1].a0), CTRL_ATTR_MCAST_GRP_ID },
				0xdeadface
			}, {
				{ sizeof(src[1].a1), CTRL_ATTR_MCAST_GRP_NAME },
				""
			}
		}
	};

	char strs[2][DEFAULT_STRLEN];
	fill_memory_ex(strs, sizeof(strs), '0', 10);
	strs[0][sizeof(strs[0]) - 1] = '\0';
	strs[1][sizeof(strs[1]) - 1] = '\0';
	memcpy(src[0].a1.s, strs[0], DEFAULT_STRLEN);
	memcpy(src[1].a1.s, strs[1], DEFAULT_STRLEN);

	void *const nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
					 NLA_HDRLEN + sizeof(src));

	JSONL_NLATTR(fd, nlh0, hdrlen,
		     init_genlmsghdr, print_genlmsghdr, CTRL_ATTR_MCAST_GROUPS,
		     sizeof(src), src, sizeof(src),
		     jsonl_array_open();
		     for (size_t i = 0; i < ARRAY_SIZE(src); ++i) {
			attr_open(!i, &src[i].h, NULL);
			jsonl_array_open();
			attr_open(true, &src[i].a0.h,
				  "CTRL_ATTR_MCAST_GRP_ID");
			jsonl_hex(src[i].a0.v);
			attr_close();
			attr_open(false, &src[i].a1.h,
				  "CTRL_ATTR_MCAST_GRP_NAME");
			jsonl_string_value(src[i].a1.s, strlen(src[i].a1.s),
					   false);
			attr_close();
			jsonl_array_close();
			attr_close();
		     }
		     jsonl_array_close());
}

enum policy_val_fmt {
	FMT_DECIMAL,
	FMT_UNSIGNED,
	FMT_HEX
};

struct policy_attr {
	uint16_t type;
	const char *sym;
	uint64_t val;
	enum policy_val_fmt fmt;
};

static void
print_policy_val(const struct policy_attr *const attr)
{
	switch (attr->fmt) {
	case FMT_DECIMAL:
		jsonl_decimal(attr->val);
		break;
	case FMT_UNSIGNED:
		jsonl_unsigned(attr->val);
		break;
	case FMT_HEX:
		jsonl_hex(attr->val);
		break;
	}
}

static void
test_nla_policy(const int fd)
{
	static const struct strval32 attr_types[] = {
		{ ARG_STR(NL_ATTR_TYPE_INVALID) },
		{ ARG_STR(NL_ATTR_TYPE_FLAG) },
		{ ARG_STR(NL_ATTR_TYPE_U8) },
		{ ARG_STR(NL_ATTR_TYPE_U16) },
		{ ARG_STR(NL_ATTR_TYPE_U32) },
		{ ARG_STR(NL_ATTR_TYPE_U64) },
		{ ARG_STR(NL_ATTR_TYPE_S8) },
		{ ARG_STR(NL_ATTR_TYPE_S16) },
		{ ARG_STR(NL_ATTR_TYPE_S32) },
		{ ARG_STR(NL_ATTR_TYPE_S64) },
		{ ARG_STR(NL_ATTR_TYPE_BINARY) },
		{ ARG_STR(NL_ATTR_TYPE_STRING) },
		{ ARG_STR(NL_ATTR_TYPE_NUL_STRING) },
		{ ARG_STR(NL_ATTR_TYPE_NESTED) },
		{ ARG_STR(NL_ATTR_TYPE_NESTED_ARRAY) },
		{ ARG_STR(NL_ATTR_TYPE_BITFIELD32) },
		{ ARG_STR(NL_ATTR_TYPE_SINT) },
		{ ARG_STR(NL_ATTR_TYPE_UINT) },
	};
	static const struct policy_attr type_attrs_64[] = {
		{ ARG_STR(NL_POLICY_TYPE_ATTR_MIN_VALUE_S),
		  -2401053088871166255LL, FMT_DECIMAL },
		{ ARG_STR(NL_POLICY_TYPE_ATTR_MAX_VALUE_S),
		  -2401053088871166254LL, FMT_DECIMAL },
		{ ARG_STR(NL_POLICY_TYPE_ATTR_MIN_VALUE_U),
		  16045690984838385363ULL, FMT_UNSIGNED },
		{ ARG_STR(NL_POLICY_TYPE_ATTR_MAX_VALUE_U),
		  16045690984838385364ULL, FMT_UNSIGNED },
		{ ARG_STR(NL_POLICY_TYPE_ATTR_MASK),
		  0xdeadbeefdefaced5ULL, FMT_HEX },
	};
	static const struct policy_attr type_attrs_32[] = {
		{ ARG_STR(NL_POLICY_TYPE_ATTR_MIN_LENGTH),
		  3740978897U, FMT_UNSIGNED },
		{ ARG_STR(NL_POLICY_TYPE_ATTR_MAX_LENGTH),
		  3740978898U, FMT_UNSIGNED },
		{ ARG_STR(NL_POLICY_TYPE_ATTR_POLICY_IDX),
		  0xdefaced3, FMT_HEX },
		{ ARG_STR(NL_POLICY_TYPE_ATTR_POLICY_MAXTYPE),
		  0xdefaced4, FMT_HEX },
		{ ARG_STR(NL_POLICY_TYPE_ATTR_BITFIELD32_MASK),
		  0xdefaced5, FMT_HEX },
	};
	static_assert(ARRAY_SIZE(type_attrs_64) == ARRAY_SIZE(type_attrs_32),
		      "type attrs size mismatch");
	struct {
		struct nlattr h;
		struct {
			struct nlattr h;
			struct {
				struct nlattr h;
				uint64_t v;
			} ATTRIBUTE_PACKED a64;
			struct u32_attr a32;
			struct u32_attr at;
		} a[ARRAY_SIZE(type_attrs_64)];
	} src[ARRAY_SIZE(attr_types)];

	for (unsigned int i = 0; i < ARRAY_SIZE(src); ++i) {
		src[i].h.nla_len = sizeof(src[i]);
		src[i].h.nla_type = NLA_F_NESTED | i;
		for (unsigned int j = 0; j < ARRAY_SIZE(src[i].a); ++j) {
			src[i].a[j].h.nla_len = sizeof(src[i].a[j]);
			src[i].a[j].h.nla_type = NLA_F_NESTED | 0x100 | j;
			src[i].a[j].a64.h.nla_len = sizeof(src[i].a[j].a64);
			src[i].a[j].a64.h.nla_type = type_attrs_64[j].type;
			src[i].a[j].a64.v = type_attrs_64[j].val;
			src[i].a[j].a32.h.nla_len = sizeof(src[i].a[j].a32);
			src[i].a[j].a32.h.nla_type = type_attrs_32[j].type;
			src[i].a[j].a32.v = type_attrs_32[j].val;
			src[i].a[j].at.h.nla_len = sizeof(src[i].a[j].at);
			src[i].a[j].at.h.nla_type = NL_POLICY_TYPE_ATTR_TYPE;
			src[i].a[j].at.v = attr_types
				[(i + j) % ARRAY_SIZE(attr_types)].val;
		}
	}

	void *const nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
					 NLA_HDRLEN + sizeof(src));

	JSONL_NLATTR_(fd, nlh0, hdrlen,
		      init_genlmsghdr, print_genlmsghdr,
		      NLA_F_NESTED | CTRL_ATTR_POLICY, "CTRL_ATTR_POLICY", NULL,
		      sizeof(src), src, sizeof(src),
		      jsonl_array_open();
		      for (unsigned int i = 0; i < ARRAY_SIZE(src); ++i) {
			attr_open(!i, &src[i].h, NULL);
			jsonl_array_open();
			for (size_t j = 0; j < ARRAY_SIZE(src[i].a); ++j) {
				const struct strval32 *const type = &attr_types
					[(i + j) % ARRAY_SIZE(attr_types)];
				struct nlattr h;

				attr_open(!j, &src[i].a[j].h, NULL);
				jsonl_array_open();
				memcpy(&h, &src[i].a[j].a64.h, sizeof(h));
				attr_open(true, &h, type_attrs_64[j].sym);
				print_policy_val(&type_attrs_64[j]);
				attr_close();
				attr_open(false, &src[i].a[j].a32.h,
					  type_attrs_32[j].sym);
				print_policy_val(&type_attrs_32[j]);
				attr_close();
				attr_open(false, &src[i].a[j].at.h,
					  "NL_POLICY_TYPE_ATTR_TYPE");
				jsonl_const(type->val, true, type->str, NULL);
				attr_close();
				jsonl_array_close();
				attr_close();
			}
			jsonl_array_close();
			attr_close();
		      }
		      jsonl_array_close());
}

static void
test_nla_op_policy(const int fd)
{
	static const struct strval32 attrs[] = {
		{ ARG_STR(CTRL_ATTR_POLICY_DO) },
		{ ARG_STR(CTRL_ATTR_POLICY_DUMP) },
	};
	const struct {
		struct nlattr h;
		struct u32_attr a[2];
	} src[] = {
		{
			{ sizeof(src[0]), NLA_F_NESTED | 0xfe },
			{
				{
					{ sizeof(src[0].a[0]), attrs[0].val },
					0xdefaced1
				}, {
					{ sizeof(src[0].a[1]), attrs[1].val },
					0xdefaced2
				}
			}
		}, {
			{ sizeof(src[1]), NLA_F_NESTED | 0xfd },
			{
				{
					{ sizeof(src[1].a[0]), attrs[0].val },
					0xdefaced3
				}, {
					{ sizeof(src[1].a[1]), attrs[1].val },
					0xdefaced4
				}
			}
		}
	};
	void *const nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
					 NLA_HDRLEN + sizeof(src));

	JSONL_NLATTR_(fd, nlh0, hdrlen,
		      init_genlmsghdr, print_genlmsghdr,
		      NLA_F_NESTED | CTRL_ATTR_OP_POLICY,
		      "CTRL_ATTR_OP_POLICY", NULL,
		      sizeof(src), src, sizeof(src),
		      jsonl_array_open();
		      for (size_t i = 0; i < ARRAY_SIZE(src); ++i) {
			attr_open(!i, &src[i].h, NULL);
			jsonl_array_open();
			for (size_t j = 0; j < ARRAY_SIZE(src[i].a); ++j) {
				attr_open(!j, &src[i].a[j].h, attrs[j].str);
				jsonl_unsigned(src[i].a[j].v);
				attr_close();
			}
			jsonl_array_close();
			attr_close();
		      }
		      jsonl_array_close());
}

/*
 * A message with CTRL_ATTR_FAMILY_NAME followed by CTRL_ATTR_OPS:
 * the commands of the latter are decoded using the tables of the family;
 * the last of cmds is an unknown command described by cmd_pattern.
 */
static void
test_ops_family(const int fd, void *const nlh0, const char *const family,
		const struct strval32 *const cmds,
		const char *const cmd_pattern)
{
	enum { NCMDS = 3 };
	static const uint32_t flags[NCMDS] = {
		GENL_CMD_CAP_DO | GENL_CMD_CAP_DUMP | GENL_CMD_CAP_HASPOL,
		GENL_ADMIN_PERM | GENL_CMD_CAP_DO | GENL_CMD_CAP_HASPOL,
		0xffffffe0
	};
	struct {
		struct nlattr h;
		struct op a[NCMDS];
	} ops = {
		.h = { sizeof(ops), CTRL_ATTR_OPS }
	};
	const unsigned int name_len = strlen(family) + 1;
	const unsigned int slen = NLA_ALIGN(name_len) + sizeof(ops);
	const unsigned int msg_len = NLMSG_SPACE(hdrlen) + NLA_HDRLEN + slen;
	struct nlmsghdr *const nlh = nlh0 - (NLA_HDRLEN + slen);
	struct nlattr *const nla = NLMSG_ATTR(nlh, hdrlen);
	char *const data = RTA_DATA(nla);

	for (unsigned int i = 0; i < NCMDS; ++i)
		init_op(&ops.a[i], i + 1, cmds[i].val, flags[i]);

	init_genlmsghdr(nlh, msg_len);
	SET_STRUCT(struct nlattr, nla,
		.nla_len = NLA_HDRLEN + name_len,
		.nla_type = CTRL_ATTR_FAMILY_NAME
	);
	memset(data, 0, NLA_ALIGN(name_len));
	memcpy(data, family, name_len);
	memcpy(data + NLA_ALIGN(name_len), &ops, sizeof(ops));

	jsonl_nl_sendto(fd, nlh, msg_len);

	jsonl_nl_sendto_open(fd);
	jsonl_array_open();
	print_genlmsghdr(msg_len);
	printf(", {");
	jsonl_array_open();
	attr_open(true, nla, "CTRL_ATTR_FAMILY_NAME");
	jsonl_string_value(family, name_len - 1, false);
	attr_close();
	attr_open(false, &ops.h, "CTRL_ATTR_OPS");
	print_ops(ops.a, NCMDS, cmds, cmd_pattern);
	attr_close();
	jsonl_array_close();
	printf("}");
	jsonl_array_close();
	jsonl_nl_sendto_close();
}

static void
test_nla_ops_family(const int fd)
{
	void *const nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
					 NLA_HDRLEN + 3 * sizeof(struct op)
					 + NLA_HDRLEN + GENL_NAMSIZ);

	static const struct strval32 devlink_cmds[] = {
		{ ARG_STR(DEVLINK_CMD_GET) },
		{ ARG_STR(DEVLINK_CMD_NOTIFY_FILTER_SET) },
		{ DEVLINK_CMD_NOTIFY_FILTER_SET + 1 },
	};
	test_ops_family(fd, nlh0, DEVLINK_GENL_NAME, devlink_cmds,
			"DEVLINK_CMD_???");

	static const struct strval32 ethtool_cmds[] = {
		{ ARG_STR(ETHTOOL_MSG_STRSET_GET) },
		{ ARG_STR(ETHTOOL_MSG_MSE_GET) },
		{ ETHTOOL_MSG_MSE_GET + 1 },
	};
	test_ops_family(fd, nlh0, ETHTOOL_GENL_NAME, ethtool_cmds,
			"ETHTOOL_MSG_???");

	static const struct strval32 ioam6_cmds[] = {
		{ ARG_STR(IOAM6_CMD_ADD_NAMESPACE) },
		{ ARG_STR(IOAM6_CMD_NS_SET_SCHEMA) },
		{ IOAM6_CMD_NS_SET_SCHEMA + 1 },
	};
	test_ops_family(fd, nlh0, IOAM6_GENL_NAME, ioam6_cmds,
			"IOAM6_CMD_???");

	static const struct strval32 mptcp_pm_cmds[] = {
		{ ARG_STR(MPTCP_PM_CMD_ADD_ADDR) },
		{ ARG_STR(MPTCP_PM_CMD_SUBFLOW_DESTROY) },
		{ MPTCP_PM_CMD_SUBFLOW_DESTROY + 1 },
	};
	test_ops_family(fd, nlh0, MPTCP_PM_NAME, mptcp_pm_cmds,
			"MPTCP_PM_CMD_???");

	static const struct strval32 netdev_cmds[] = {
		{ ARG_STR(NETDEV_CMD_DEV_GET) },
		{ ARG_STR(NETDEV_CMD_QUEUE_CREATE) },
		{ NETDEV_CMD_QUEUE_CREATE + 1 },
	};
	test_ops_family(fd, nlh0, NETDEV_FAMILY_NAME, netdev_cmds,
			"NETDEV_CMD_???");

	static const struct strval32 nl80211_cmds[] = {
		{ ARG_STR(NL80211_CMD_GET_WIPHY) },
		{ ARG_STR(NL80211_CMD_STOP_PD) },
		{ NL80211_CMD_STOP_PD + 1 },
	};
	test_ops_family(fd, nlh0, NL80211_GENL_NAME, nl80211_cmds,
			"NL80211_CMD_???");

	static const struct strval32 seg6_cmds[] = {
		{ ARG_STR(SEG6_CMD_SETHMAC) },
		{ ARG_STR(SEG6_CMD_GET_TUNSRC) },
		{ SEG6_CMD_GET_TUNSRC + 1 },
	};
	test_ops_family(fd, nlh0, SEG6_GENL_NAME, seg6_cmds,
			"SEG6_CMD_???");

	static const struct strval32 taskstats_cmds[] = {
		{ ARG_STR(TASKSTATS_CMD_GET) },
		{ ARG_STR(CGROUPSTATS_CMD_NEW) },
		{ CGROUPSTATS_CMD_NEW + 1 },
	};
	test_ops_family(fd, nlh0, TASKSTATS_GENL_NAME, taskstats_cmds,
			"TASKSTATS_CMD_???");

	static const struct strval32 tcp_metrics_cmds[] = {
		{ ARG_STR(TCP_METRICS_CMD_GET) },
		{ ARG_STR(TCP_METRICS_CMD_DEL) },
		{ TCP_METRICS_CMD_DEL + 1 },
	};
	test_ops_family(fd, nlh0, TCP_METRICS_GENL_NAME, tcp_metrics_cmds,
			"TCP_METRICS_CMD_???");

	static const struct strval32 thermal_cmds[] = {
		{ ARG_STR(THERMAL_GENL_CMD_TZ_GET_ID) },
		{ ARG_STR(THERMAL_GENL_CMD_THRESHOLD_FLUSH) },
		{ THERMAL_GENL_CMD_THRESHOLD_FLUSH + 1 },
	};
	test_ops_family(fd, nlh0, THERMAL_GENL_FAMILY_NAME, thermal_cmds,
			"THERMAL_GENL_CMD_???");
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
	void *const nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
					 NLA_HDRLEN + DEFAULT_STRLEN);

	test_hdr(fd);
	test_nla_unknown(fd, nlh0);
	test_nla_x16(fd, nlh0);
	test_nla_u32(fd, nlh0);
	test_nla_str(fd, nlh0);
	test_nla_ops(fd);
	test_nla_mcast(fd);
	test_nla_policy(fd);
	test_nla_op_policy(fd);
	test_nla_ops_family(fd);
	test_nlmsg_done(fd);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
