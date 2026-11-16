/*
 * Check JSONL output of IFLA_LINKINFO netlink attribute.
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

# include <stddef.h>
# include <arpa/inet.h>
# include <linux/if.h>
# include <linux/if_arp.h>
# include <linux/if_bridge.h>
# include <linux/if_link.h>

static const unsigned int hdrlen = sizeof(struct ifinfomsg);
static char pattern[4096];

static void
init_ifinfomsg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = RTM_GETLINK,
		.nlmsg_flags = NLM_F_DUMP
	);

	struct ifinfomsg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct ifinfomsg, msg,
		.ifi_family = AF_UNIX,
		.ifi_type = ARPHRD_LOOPBACK,
		.ifi_index = ifindex_lo(),
		.ifi_flags = IFF_UP,
	);
}

static void
print_ifinfomsg(const unsigned int msg_len)
{
	static const struct strval32 nlmsg_flags[] = {
		{ ARG_STR(NLM_F_DUMP) }
	};
	static const struct strval32 ifi_flags[] = {
		{ ARG_STR(IFF_UP) }
	};

	printf("{");
	jsonl_nlmsghdr_vals(msg_len, ARG_STR(RTM_GETLINK), NULL,
			    NLM_F_DUMP, nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	jsonl_struct_open();
	jsonl_field_open("ifi_family", true);
	jsonl_const(AF_UNIX, true, "AF_UNIX", NULL);
	jsonl_field_close();
	jsonl_field_open("ifi_type", false);
	jsonl_const(ARPHRD_LOOPBACK, true, "ARPHRD_LOOPBACK", NULL);
	jsonl_field_close();
	jsonl_field_open("ifi_index", false);
	jsonl_ifindex(ifindex_lo(), "lo");
	jsonl_field_close();
	jsonl_field_open("ifi_flags", false);
	jsonl_flags(IFF_UP, ifi_flags, ARRAY_SIZE(ifi_flags), NULL);
	jsonl_field_close();
	jsonl_field_open("ifi_change", false);
	jsonl_hex(0);
	jsonl_field_close();
	jsonl_struct_close();
	printf("}");
}

static void
init_linkinfo_msg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	init_ifinfomsg(nlh, msg_len);

	struct nlattr *const nla = NLMSG_ATTR(nlh, hdrlen);
	SET_STRUCT(struct nlattr, nla,
		.nla_len = msg_len - NLMSG_SPACE(hdrlen),
		.nla_type = IFLA_LINKINFO
	);
}

static void
print_linkinfo_msg(const unsigned int msg_len)
{
	print_ifinfomsg(msg_len);
	jsonl_nlattr_nest_open(msg_len - NLMSG_SPACE(hdrlen),
			       ARG_STR(IFLA_LINKINFO), NULL);
}

/* Print the fields of the value of an attribute payload of sz bytes. */
typedef void (*print_val_fn)(const void *p, unsigned int sz);

/*
 * The payload of the IFLA_LINKINFO attribute being checked: an attribute
 * with the kind of the link followed by one more attribute.
 */
static char li_buf[256];
static struct {
	struct strval32 kind_attr;
	const char *kind;
	struct strval32 attr;
	const char *attr_pattern;
	unsigned int attr_off;
	unsigned int data_len;
	print_val_fn print;
	unsigned int val_len;
} li;

/* The tables used by the functions that print const and flags values. */
static const struct strval32 *cur_xlat;
static size_t cur_xlat_size;
static const char *cur_dflt;

/* The attributes nested in the second attribute of li_buf. */
struct sub_attr {
	unsigned int sz;
	print_val_fn print;
};
static struct strval32 sub_type;
static const char *sub_pattern;
static const struct sub_attr *subs;
static size_t nsubs;

static void
set_xlat(const struct strval32 *const xlat, const size_t size,
	 const char *const dflt)
{
	cur_xlat = xlat;
	cur_xlat_size = size;
	cur_dflt = dflt;
}

/* Return the offset of the payload of the second attribute in li_buf. */
static unsigned int
li_init(const struct strval32 kind_attr, const char *const kind,
	const struct strval32 attr, const char *const attr_pattern,
	const unsigned int data_len)
{
	const size_t kind_len = strlen(kind) + 1;
	struct nlattr nla = {
		.nla_len = NLA_HDRLEN + kind_len,
		.nla_type = kind_attr.val
	};

	memset(li_buf, 0, sizeof(li_buf));
	memcpy(li_buf, &nla, sizeof(nla));
	memcpy(li_buf + NLA_HDRLEN, kind, kind_len);

	li.kind_attr = kind_attr;
	li.kind = kind;
	li.attr = attr;
	li.attr_pattern = attr_pattern;
	li.attr_off = NLA_ALIGN(nla.nla_len);
	li.data_len = data_len;

	nla.nla_len = NLA_HDRLEN + data_len;
	nla.nla_type = attr.val;
	memcpy(li_buf + li.attr_off, &nla, sizeof(nla));

	if (li.attr_off + nla.nla_len > sizeof(li_buf))
		error_msg_and_fail("li_buf is too small");

	return li.attr_off + NLA_HDRLEN;
}

static void
print_linkinfo(void)
{
	const size_t kind_len = strlen(li.kind);

	jsonl_array_open();
	printf("{");
	jsonl_nlattr_open(NLA_HDRLEN + kind_len + 1,
			  li.kind_attr.val, li.kind_attr.str, NULL);
	jsonl_string_value(li.kind, kind_len, false);
	jsonl_nlattr_close();
	printf("}, {");
	jsonl_nlattr_open(NLA_HDRLEN + li.data_len,
			  li.attr.val, li.attr.str, li.attr_pattern);
	li.print(li_buf + li.attr_off + NLA_HDRLEN, li.val_len);
	jsonl_nlattr_close();
	printf("}");
	jsonl_array_close();
}

/*
 * Send IFLA_LINKINFO with the payload prepared in li_buf, without its last
 * cut bytes; the last unreadable bytes of the payload cannot be read.
 * The value of the second attribute is printed by print(ptr, val_len).
 */
static void
check_li(const int fd, void *const nlh0,
	 const unsigned int cut, const unsigned int unreadable,
	 const print_val_fn print, const unsigned int val_len)
{
	const unsigned int len =
		li.attr_off + NLA_HDRLEN + li.data_len - cut;

	li.print = print;
	li.val_len = val_len;
	JSONL_NLATTR(fd, nlh0, hdrlen, init_ifinfomsg, print_ifinfomsg,
		     IFLA_LINKINFO, len, li_buf, len - unreadable,
		     print_linkinfo());
}

static void
pr_bytes(const void *p, unsigned int sz)
{
	jsonl_bytes_value(p, sz, false);
}

static void
pr_unterminated_string(const void *p, unsigned int sz)
{
	jsonl_string_value(p, sz, true);
}

static void
pr_addr(const void *p, unsigned int sz)
{
	jsonl_addr(jsonl_nl_data + ((const char *) p - li_buf));
}

static void
pr_mac(const void *p, unsigned int sz)
{
	jsonl_mac(p, sz);
}

/* Fetch an integer of at most sz bytes the way the decoders do. */
static uint64_t
get_uint(const void *p, unsigned int sz, unsigned int max_sz)
{
	union {
		uint8_t u8;
		uint16_t u16;
		uint32_t u32;
		uint64_t u64;
		char bytes[8];
	} u = { .u64 = 0 };

	sz = MIN(sz, max_sz);
	if (max_sz == 8) {
		memcpy(u.bytes + BE_LE(8 - sz, 0), p, sz);
		return u.u64;
	}

	memcpy(u.bytes, p, sz);
	switch (max_sz) {
	case 1: return u.u8;
	case 2: return u.u16;
	default: return u.u32;
	}
}

static void
pr_u8(const void *p, unsigned int sz)
{
	jsonl_unsigned(get_uint(p, sz, 1));
}

static void
pr_u16(const void *p, unsigned int sz)
{
	jsonl_unsigned(get_uint(p, sz, 2));
}

static void
pr_x16(const void *p, unsigned int sz)
{
	jsonl_hex(get_uint(p, sz, 2));
}

static void
pr_u32(const void *p, unsigned int sz)
{
	jsonl_unsigned(get_uint(p, sz, 4));
}

static void
pr_s32(const void *p, unsigned int sz)
{
	jsonl_decimal((int32_t) get_uint(p, sz, 4));
}

static void
pr_ticks(const void *p, unsigned int sz)
{
	jsonl_ticks(get_uint(p, sz, 8));
}

static void
pr_uid(const void *p, unsigned int sz)
{
	const uint32_t uid = get_uint(p, sz, 4);

	printf("\"type\": \"uid\", \"raw\": \"");
	if (uid == -1U)
		printf("-1");
	else
		printf("%u", uid);
	printf("\"");
}

static void
pr_ifindex(const void *p, unsigned int sz)
{
	const uint32_t idx = get_uint(p, sz, 4);

	jsonl_ifindex(idx, idx == ifindex_lo() ? "lo" : NULL);
}

static void
pr_const(const uint32_t val)
{
	const char *sym = NULL;

	for (size_t i = 0; i < cur_xlat_size; ++i) {
		if (cur_xlat[i].val == val) {
			sym = cur_xlat[i].str;
			break;
		}
	}
	jsonl_const(val, true, sym, cur_dflt);
}

static void
pr_const8(const void *p, unsigned int sz)
{
	pr_const(get_uint(p, sz, 1));
}

static void
pr_const32(const void *p, unsigned int sz)
{
	pr_const(get_uint(p, sz, 4));
}

static void
pr_flags16(const void *p, unsigned int sz)
{
	jsonl_flags(get_uint(p, sz, 2), cur_xlat, cur_xlat_size, cur_dflt);
}

static void
pr_eth_p_preauth(const void *p, unsigned int sz)
{
	jsonl_const(ntohs(get_uint(p, sz, 2)), true, "ETH_P_PREAUTH", NULL);
	jsonl_byteorder_big(2);
}

static void
pr_bridge_id(const void *p, unsigned int sz)
{
	const uint8_t *const id = p;

	jsonl_struct_open();
	jsonl_field_open("prio", true);
	jsonl_array_open();
	printf("{");
	jsonl_unsigned(id[0]);
	printf("}, {");
	jsonl_unsigned(id[1]);
	printf("}");
	jsonl_array_close();
	jsonl_field_close();
	jsonl_field_open("addr", false);
	jsonl_mac(id + 2, 6);
	jsonl_field_close();
	jsonl_struct_close();
}

static void
pr_zero_boolopt(const void *p, unsigned int sz)
{
	jsonl_struct_open();
	jsonl_field_open("optval", true);
	jsonl_flags(0, NULL, 0, NULL);
	jsonl_field_close();
	jsonl_field_open("optmask", false);
	jsonl_flags(0, NULL, 0, NULL);
	jsonl_field_close();
	jsonl_struct_close();
}

static void
pr_can_stats(const void *p, unsigned int sz)
{
	static const char *const names[] = {
		"bus_error", "error_warning", "error_passive",
		"bus_off", "arbitration_lost", "restarts"
	};
	uint32_t vals[ARRAY_SIZE(names)];

	memcpy(vals, p, sizeof(vals));
	jsonl_struct_open();
	for (size_t i = 0; i < ARRAY_SIZE(names); ++i) {
		jsonl_field_open(names[i], i == 0);
		jsonl_unsigned(vals[i]);
		jsonl_field_close();
	}
	jsonl_struct_close();
}

/* The BRIDGE_QUERIER_* attribute nested in IFLA_BR_MCAST_QUERIER_STATE. */
static struct {
	struct strval32 type;
	unsigned int data_len;
	print_val_fn print;
	print_val_fn print_cropped;
} qstate;

static void
pr_ipv4(const void *p, unsigned int sz)
{
	char buf[INET_ADDRSTRLEN];

	jsonl_ip_addr(AF_INET, "AF_INET",
		      inet_ntop(AF_INET, p, buf, sizeof(buf)));
}

static void
pr_ipv6(const void *p, unsigned int sz)
{
	char buf[INET6_ADDRSTRLEN];

	jsonl_ip_addr(AF_INET6, "AF_INET6",
		      inet_ntop(AF_INET6, p, buf, sizeof(buf)));
}

static void
pr_qstate(const void *p, unsigned int sz)
{
	const unsigned int nla_len = NLA_HDRLEN + qstate.data_len;

	if (sz == NLA_HDRLEN) {
		jsonl_nlattr_hdr(nla_len, qstate.type.val, qstate.type.str,
				 "BRIDGE_QUERIER_???");
		return;
	}

	jsonl_nlattr_open(nla_len, qstate.type.val, qstate.type.str,
			  "BRIDGE_QUERIER_???");
	(sz == nla_len ? qstate.print : qstate.print_cropped)
		((const char *) p + NLA_HDRLEN, sz - NLA_HDRLEN);
	jsonl_nlattr_close();
}

/* The second attribute nested in IFLA_MACVLAN_MACADDR_DATA is unknown. */
static bool macaddr_data_unknown;

static void
pr_macaddr_data(const void *p, unsigned int sz)
{
	const unsigned int mac_len = 6;
	const unsigned int nla_len = NLA_HDRLEN + mac_len;
	const unsigned int next = NLA_ALIGN(nla_len);
	const char *const second = (const char *) p + next;

	if (sz > next) {
		jsonl_array_open();
		printf("{");
	}
	jsonl_nlattr_open(nla_len, ARG_STR(IFLA_MACVLAN_MACADDR), NULL);
	jsonl_mac((const char *) p + NLA_HDRLEN, mac_len);
	jsonl_nlattr_close();
	if (sz <= next)
		return;

	printf("}, {");
	if (sz - next < nla_len) {
		jsonl_bytes_value(second, sz - next, false);
	} else if (macaddr_data_unknown) {
		jsonl_nlattr_open(nla_len, IFLA_MACVLAN_MAX + 1, NULL,
				  "IFLA_MACVLAN_???");
		jsonl_bytes_value(second + NLA_HDRLEN, mac_len, false);
		jsonl_nlattr_close();
	} else {
		jsonl_nlattr_open(nla_len, ARG_STR(IFLA_MACVLAN_MACADDR),
				  NULL);
		jsonl_mac(second + NLA_HDRLEN, mac_len);
		jsonl_nlattr_close();
	}
	printf("}");
	jsonl_array_close();
}

static void
pr_subs(const void *p, unsigned int sz)
{
	const char *pos = p;

	if (nsubs > 1)
		jsonl_array_open();
	for (size_t i = 0; i < nsubs; ++i) {
		const unsigned int nla_len = NLA_HDRLEN + subs[i].sz;

		if (nsubs > 1)
			printf("%s{", i ? ", " : "");
		if (subs[i].print) {
			jsonl_nlattr_open(nla_len, sub_type.val, sub_type.str,
					  sub_pattern);
			subs[i].print(pos + NLA_HDRLEN, subs[i].sz);
			jsonl_nlattr_close();
		} else {
			jsonl_nlattr_hdr(nla_len, sub_type.val, sub_type.str,
					 sub_pattern);
		}
		if (nsubs > 1)
			printf("}");
		pos += NLA_ALIGN(nla_len);
	}
	if (nsubs > 1)
		jsonl_array_close();
}

/*
 * Check attributes of the same type nested in the second attribute of
 * IFLA_LINKINFO: the payload of each one consists of the first bytes
 * of obj, followed by the bytes of pattern when it is longer than obj.
 */
static void
check_nested(const int fd, void *const nlh0,
	     const struct strval32 kind_attr, const struct strval32 data_attr,
	     const char *const kind,
	     const struct strval32 type, const char *const type_pattern,
	     const void *const obj, const unsigned int objsz,
	     const struct sub_attr *const attrs, const size_t n)
{
	unsigned int attrsz = 0;

	for (size_t i = 0; i < n; ++i)
		attrsz += NLA_HDRLEN + NLA_ALIGN(attrs[i].sz);

	char *pos = li_buf + li_init(kind_attr, kind, data_attr, NULL, attrsz);

	for (size_t i = 0; i < n; ++i) {
		const struct nlattr nla = {
			.nla_len = NLA_HDRLEN + attrs[i].sz,
			.nla_type = type.val
		};

		memcpy(pos, &nla, sizeof(nla));
		memcpy(pos + NLA_HDRLEN, obj, MIN(objsz, attrs[i].sz));
		if (attrs[i].sz > objsz)
			memcpy(pos + NLA_HDRLEN + objsz, pattern,
			       attrs[i].sz - objsz);
		pos += NLA_ALIGN(nla.nla_len);
	}

	sub_type = type;
	sub_pattern = type_pattern;
	subs = attrs;
	nsubs = n;
	check_li(fd, nlh0, 0, 0, pr_subs, attrsz);
}

static const struct strval32 info_kind = { ARG_STR(IFLA_INFO_KIND) };
static const struct strval32 info_data = { ARG_STR(IFLA_INFO_DATA) };
static const struct strval32 info_slave_kind = {
	ARG_STR(IFLA_INFO_SLAVE_KIND)
};
static const struct strval32 info_slave_data = {
	ARG_STR(IFLA_INFO_SLAVE_DATA)
};

# define CHECK_NESTED(kind_attr_, data_attr_, kind_,			\
		      type_, sym_, pattern_, obj_, ...)			\
	do {								\
		const struct sub_attr attrs_[] = { __VA_ARGS__ };	\
		check_nested(fd, nlh0, (kind_attr_), (data_attr_), (kind_), \
			     (struct strval32) { (type_), (sym_) },	\
			     (pattern_), &(obj_), sizeof(obj_),		\
			     attrs_, ARRAY_SIZE(attrs_));		\
	} while (0)

# define CHECK_DATA(kind_, type_, sym_, pattern_, obj_, ...)		\
	CHECK_NESTED(info_kind, info_data, (kind_),			\
		     (type_), (sym_), (pattern_), obj_, __VA_ARGS__)

# define CHECK_SLAVE_DATA(kind_, type_, sym_, pattern_, obj_, ...)	\
	CHECK_NESTED(info_slave_kind, info_slave_data, (kind_),		\
		     (type_), (sym_), (pattern_), obj_, __VA_ARGS__)

static const uint8_t unknown_msg[] = { 0xab, 0xac, 0xdb, 0xcd };

/*
 * Check the attribute of the given type, with unknown_msg as its payload,
 * that follows the attribute with each of the listed kinds.
 */
static void
check_kinds(const int fd, void *const nlh0, const struct strval32 kind_attr,
	    const unsigned int type, const char *const sym,
	    const char *const *const *lists, const print_val_fn print)
{
	for (; *lists; ++lists) {
		for (const char *const *kind = *lists; *kind; ++kind) {
			const unsigned int off =
				li_init(kind_attr, *kind,
					(struct strval32) { type, sym },
					"IFLA_INFO_???", sizeof(unknown_msg));

			memcpy(li_buf + off, unknown_msg, sizeof(unknown_msg));
			check_li(fd, nlh0, 0, 0, print, sizeof(unknown_msg));
		}
	}
}

int
main(int argc, char *argv[])
{
	static const char *const unsupported_tunnel_types[] = {
		"batadv", "bareudp", "bond",
		"caif", "cfhsi",
		"dummy",
		"erspan",
		"geneve", "gre", "gretap", "gtp",
		"hsr",
		"ifb", "ip6erspan", "ip6gre", "ip6gretap", "ip6tnl",
		"ipip", "ipoib", "ipvlan", "ipvtap",
		"lowpan",
		"macsec", "macvtap",
		"netdevsim", "nlmon",
		"openvswitch",
		"ppp",
		"rmnet",
		"sit",
		"team",
		"vcan", "veth", "vlan", "vrf", "vsockmon",
		"vti", "vti6", "vxcan", "vxlan",
		"wireguard", "wwan",
		"xfrm",
		NULL
	};
	static const char *const unsupported_xstats_types[] = {
		"bridge",
		"macvlan",
		"tun",
		NULL
	};
	static const char *const unsupported_data_types[] = {
		"can",
		NULL
	};
	static const char *const unsupported_slave_data_types[] = {
		"can",
		"tun",
		NULL
	};
	/* supported by at least one attribute */
	static const char *const supported_tunnel_types[] = {
		"bridge",
		"can",
		"macvlan",
		"tun",
		NULL
	};
	static const char *const *const all_types[] = {
		unsupported_tunnel_types, supported_tunnel_types, NULL
	};
	static const char *const *const no_data_types[] = {
		unsupported_tunnel_types, unsupported_data_types, NULL
	};
	static const char *const *const no_xstats_types[] = {
		unsupported_tunnel_types, unsupported_xstats_types,
		/* can xstats are decoded only if they are big enough */
		unsupported_data_types, NULL
	};
	static const char *const *const no_slave_data_types[] = {
		unsupported_tunnel_types, unsupported_slave_data_types, NULL
	};

	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
				   2 * NLA_HDRLEN + sizeof(li_buf));

	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	/* undecoded and unknown IFLA_INFO_* types */
	JSONL_NESTED_NLATTR_OBJECT(fd, nlh0, hdrlen,
				   init_linkinfo_msg, print_linkinfo_msg,
				   IFLA_INFO_UNSPEC, pattern, unknown_msg,
				   pr_bytes(unknown_msg, sizeof(unknown_msg)));

	JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
				       init_linkinfo_msg, print_linkinfo_msg,
				       6, NULL, "IFLA_INFO_???", pattern,
				       unknown_msg, jsonl_nl_bytes, 1,
				       pr_bytes(unknown_msg,
						sizeof(unknown_msg)));

	/* IFLA_INFO_KIND and IFLA_INFO_SLAVE_KIND alone */
	JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
				       init_linkinfo_msg, print_linkinfo_msg,
				       IFLA_INFO_KIND, "IFLA_INFO_KIND", NULL,
				       pattern, unknown_msg,
				       pr_unterminated_string, 1,
				       pr_unterminated_string(unknown_msg,
						sizeof(unknown_msg)));

	JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
				       init_linkinfo_msg, print_linkinfo_msg,
				       IFLA_INFO_SLAVE_KIND,
				       "IFLA_INFO_SLAVE_KIND", NULL,
				       pattern, unknown_msg,
				       pr_unterminated_string, 1,
				       pr_unterminated_string(unknown_msg,
						sizeof(unknown_msg)));

	/* IFLA_INFO_KIND followed by an attribute that is not decoded */
	check_kinds(fd, nlh0, info_kind, ARG_STR(IFLA_INFO_UNSPEC),
		    all_types, pr_bytes);
	check_kinds(fd, nlh0, info_kind, ARG_STR(IFLA_INFO_KIND),
		    all_types, pr_unterminated_string);
	check_kinds(fd, nlh0, info_kind, ARG_STR(IFLA_INFO_DATA),
		    no_data_types, pr_bytes);
	check_kinds(fd, nlh0, info_kind, ARG_STR(IFLA_INFO_XSTATS),
		    no_xstats_types, pr_bytes);
	check_kinds(fd, nlh0, info_kind, ARG_STR(IFLA_INFO_SLAVE_KIND),
		    all_types, pr_unterminated_string);
	check_kinds(fd, nlh0, info_kind, ARG_STR(IFLA_INFO_SLAVE_DATA),
		    all_types, pr_bytes);
	check_kinds(fd, nlh0, info_kind, 6, NULL, all_types, pr_bytes);

	/* IFLA_INFO_SLAVE_KIND followed by such an attribute */
	check_kinds(fd, nlh0, info_slave_kind, ARG_STR(IFLA_INFO_UNSPEC),
		    all_types, pr_bytes);
	check_kinds(fd, nlh0, info_slave_kind, ARG_STR(IFLA_INFO_KIND),
		    all_types, pr_unterminated_string);
	check_kinds(fd, nlh0, info_slave_kind, ARG_STR(IFLA_INFO_DATA),
		    no_data_types, pr_bytes);
	check_kinds(fd, nlh0, info_slave_kind, ARG_STR(IFLA_INFO_SLAVE_DATA),
		    no_slave_data_types, pr_bytes);
	check_kinds(fd, nlh0, info_slave_kind, 6, NULL, all_types, pr_bytes);

	static const uint64_t u64_val = 0xdeadc0defacefeedULL;
	static const uint32_t u32_val = 0xbadc0dedU;
	static const uint16_t u16_val = 0xdeed;
	static const uint8_t  u8_val  = 0xa1;

	/* bridge attrs */
	static const struct strval32 und_br_attrs[] = {
		{ 0, "IFLA_BR_UNSPEC" },
		{ 21, "IFLA_BR_FDB_FLUSH" },
		{ 40, "IFLA_BR_PAD" },
		{ 51, NULL },
	};

	for (size_t k = 0; k < ARRAY_SIZE(und_br_attrs); k++) {
		CHECK_DATA("bridge",
			   und_br_attrs[k].val, und_br_attrs[k].str,
			   "IFLA_BR_???", unknown_msg,
			   { 2, pr_bytes },
			   { 4, pr_bytes },
			   { 6, pr_bytes },
			   { 8, pr_bytes },
			   { 10, pr_bytes });
	}

	CHECK_DATA("bridge", 20, "IFLA_BR_GROUP_ADDR", NULL, unknown_msg,
		   { 2, pr_mac },
		   { 4, pr_mac },
		   { 6, pr_mac },
		   { 8, pr_mac },
		   { 10, pr_mac });

	static const struct strval32 c_t_br_attrs[] = {
		{  1, "IFLA_BR_FORWARD_DELAY" },
		{  2, "IFLA_BR_HELLO_TIME" },
		{  3, "IFLA_BR_MAX_AGE" },
		{  4, "IFLA_BR_AGEING_TIME" },
		{ 16, "IFLA_BR_HELLO_TIMER" },
		{ 17, "IFLA_BR_TCN_TIMER" },
		{ 18, "IFLA_BR_TOPOLOGY_CHANGE_TIMER" },
		{ 19, "IFLA_BR_GC_TIMER" },
		{ 30, "IFLA_BR_MCAST_LAST_MEMBER_INTVL" },
		{ 31, "IFLA_BR_MCAST_MEMBERSHIP_INTVL" },
		{ 32, "IFLA_BR_MCAST_QUERIER_INTVL" },
		{ 33, "IFLA_BR_MCAST_QUERY_INTVL" },
		{ 34, "IFLA_BR_MCAST_QUERY_RESPONSE_INTVL" },
		{ 35, "IFLA_BR_MCAST_STARTUP_QUERY_INTVL" },
	};

	for (size_t k = 0; k < ARRAY_SIZE(c_t_br_attrs); k++) {
		CHECK_DATA("bridge",
			   c_t_br_attrs[k].val, c_t_br_attrs[k].str, NULL,
			   u64_val,
			   { 7, pr_ticks },
			   { 8, pr_ticks },
			   { 9, pr_bytes });
	}

	static const struct strval32 u32_br_attrs[] = {
		{  5, "IFLA_BR_STP_STATE" },
		{ 13, "IFLA_BR_ROOT_PATH_COST" },
		{ 26, "IFLA_BR_MCAST_HASH_ELASTICITY" },
		{ 27, "IFLA_BR_MCAST_HASH_MAX" },
		{ 28, "IFLA_BR_MCAST_LAST_MEMBER_CNT" },
		{ 29, "IFLA_BR_MCAST_STARTUP_QUERY_CNT" },
		{ 48, "IFLA_BR_FDB_N_LEARNED" },
		{ 49, "IFLA_BR_FDB_MAX_LEARNED" },
	};

	for (size_t k = 0; k < ARRAY_SIZE(u32_br_attrs); k++) {
		CHECK_DATA("bridge",
			   u32_br_attrs[k].val, u32_br_attrs[k].str, NULL,
			   u32_val,
			   { 3, pr_bytes },
			   { 4, pr_u32 },
			   { 5, pr_u32 });
	}

	static const struct strval32 stp_modes[] = {
		{ 0, "BR_STP_MODE_AUTO" },
		{ 1, "BR_STP_MODE_USER" },
		{ 2, "BR_STP_MODE_KERNEL" },
		{ 0xbadc0dedU, NULL },
	};

	set_xlat(stp_modes, ARRAY_SIZE(stp_modes) - 1, "BR_STP_MODE_???");
	for (size_t k = 0; k < ARRAY_SIZE(stp_modes); k++) {
		CHECK_DATA("bridge", 50, "IFLA_BR_STP_MODE", NULL,
			   stp_modes[k].val,
			   { 3, pr_bytes },
			   { 4, pr_const32 },
			   { 5, pr_const32 });
	}

	static const struct strval32 u16_br_attrs[] = {
		{  6, "IFLA_BR_PRIORITY" },
		{ 12, "IFLA_BR_ROOT_PORT" },
		{ 39, "IFLA_BR_VLAN_DEFAULT_PVID" },
	};

	for (size_t k = 0; k < ARRAY_SIZE(u16_br_attrs); k++) {
		CHECK_DATA("bridge",
			   u16_br_attrs[k].val, u16_br_attrs[k].str, NULL,
			   u16_val,
			   { 1, pr_bytes },
			   { 2, pr_u16 },
			   { 3, pr_u16 });
	}

	CHECK_DATA("bridge", 9, "IFLA_BR_GROUP_FWD_MASK", NULL, u16_val,
		   { 1, pr_bytes },
		   { 2, pr_x16 },
		   { 3, pr_x16 });

	static const struct strval32 u8_br_attrs[] = {
		{  7, "IFLA_BR_VLAN_FILTERING" },
		{ 14, "IFLA_BR_TOPOLOGY_CHANGE" },
		{ 15, "IFLA_BR_TOPOLOGY_CHANGE_DETECTED" },
		{ 22, "IFLA_BR_MCAST_ROUTER" },
		{ 23, "IFLA_BR_MCAST_SNOOPING" },
		{ 24, "IFLA_BR_MCAST_QUERY_USE_IFADDR" },
		{ 25, "IFLA_BR_MCAST_QUERIER" },
		{ 36, "IFLA_BR_NF_CALL_IPTABLES" },
		{ 37, "IFLA_BR_NF_CALL_IP6TABLES" },
		{ 38, "IFLA_BR_NF_CALL_ARPTABLES" },
		{ 41, "IFLA_BR_VLAN_STATS_ENABLED" },
		{ 42, "IFLA_BR_MCAST_STATS_ENABLED" },
		{ 43, "IFLA_BR_MCAST_IGMP_VERSION" },
		{ 44, "IFLA_BR_MCAST_MLD_VERSION" },
		{ 45, "IFLA_BR_VLAN_STATS_PER_PORT" },
	};

	for (size_t k = 0; k < ARRAY_SIZE(u8_br_attrs); k++) {
		CHECK_DATA("bridge",
			   u8_br_attrs[k].val, u8_br_attrs[k].str, NULL,
			   u8_val,
			   { 0, NULL },
			   { 1, pr_u8 },
			   { 2, pr_u8 });
	}

	const uint16_t eth_p = htons(0x88C7);
	CHECK_DATA("bridge", 8, "IFLA_BR_VLAN_PROTOCOL", NULL, eth_p,
		   { 1, pr_bytes },
		   { 2, pr_eth_p_preauth },
		   { 2, pr_eth_p_preauth });

	static const uint8_t bridge_id[] = {
		0xbe, 0xef, 0xfa, 0xce, 0xde, 0xc0, 0xde, 0xad
	};
	static const struct strval32 br_id_attrs[] = {
		{ 10, "IFLA_BR_ROOT_ID" },
		{ 11, "IFLA_BR_BRIDGE_ID" },
	};

	for (size_t k = 0; k < ARRAY_SIZE(br_id_attrs); k++) {
		CHECK_DATA("bridge",
			   br_id_attrs[k].val, br_id_attrs[k].str, NULL,
			   bridge_id,
			   { 7, pr_bytes },
			   { 8, pr_bridge_id },
			   { 9, pr_bridge_id });
	}

	static const struct br_boolopt_multi zero_boolopt;
	CHECK_DATA("bridge", 46, "IFLA_BR_MULTI_BOOLOPT", NULL, zero_boolopt,
		   { 7, pr_bytes },
		   { 9, pr_zero_boolopt },
		   { 9, pr_zero_boolopt });

	/* IFLA_BR_MCAST_QUERIER_STATE */
	const struct {
		struct strval32 type;
		unsigned int data_len;
		print_val_fn print;
		print_val_fn print_cropped;
		uint8_t data[16];
		uint32_t u32;
	} qstate_attrs[] = {
		{ { ARG_STR(BRIDGE_QUERIER_UNSPEC) }, 4, pr_bytes, pr_bytes,
		  { 0xde, 0xad, 0xfa, 0xce } },
		{ { ARG_STR(BRIDGE_QUERIER_IP_ADDRESS) }, 4, pr_ipv4, pr_bytes,
		  { 0x5d, 0xb8, 0xd8, 0x22 } },
		{ { ARG_STR(BRIDGE_QUERIER_IP_PORT) }, 4, pr_ifindex, pr_bytes,
		  .u32 = ifindex_lo() },
		{ { ARG_STR(BRIDGE_QUERIER_IP_OTHER_TIMER) }, 4,
		  pr_ticks, pr_ticks, .u32 = 0xcafefeed },
		{ { ARG_STR(BRIDGE_QUERIER_PAD) }, 4, pr_bytes, pr_bytes,
		  { 0xfa, 0xce, 0xfe, 0xed } },
		{ { ARG_STR(BRIDGE_QUERIER_IPV6_ADDRESS) }, 16,
		  pr_ipv6, pr_bytes,
		  { 0xde, 0xad, 0xfa, 0xce, 0x80, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00, 0xad, 0x00, 0x00, 0x00, 0xec } },
		{ { ARG_STR(BRIDGE_QUERIER_IPV6_PORT) }, 4,
		  pr_ifindex, pr_bytes, .u32 = 3141592653U },
		{ { ARG_STR(BRIDGE_QUERIER_IPV6_OTHER_TIMER) }, 4,
		  pr_ticks, pr_ticks, .u32 = 0xfacebeef },
		{ { 8, NULL }, 1, pr_bytes, NULL, { 0x69 } },
	};
	for (size_t k = 0; k < ARRAY_SIZE(qstate_attrs); k++) {
		struct {
			struct nlattr nla;
			uint8_t data[16];
		} val = {
			.nla = {
				.nla_len = NLA_HDRLEN
					   + qstate_attrs[k].data_len,
				.nla_type = qstate_attrs[k].type.val
			}
		};

		if (qstate_attrs[k].u32)
			memcpy(val.data, &qstate_attrs[k].u32,
			       sizeof(qstate_attrs[k].u32));
		else
			memcpy(val.data, qstate_attrs[k].data,
			       sizeof(val.data));

		qstate.type = qstate_attrs[k].type;
		qstate.data_len = qstate_attrs[k].data_len;
		qstate.print = qstate_attrs[k].print;
		qstate.print_cropped = qstate_attrs[k].print_cropped;

		CHECK_DATA("bridge", 47, "IFLA_BR_MCAST_QUERIER_STATE", NULL,
			   val,
			   { val.nla.nla_len - 1, pr_qstate },
			   { val.nla.nla_len, pr_qstate });
	}

	/* macvlan attrs */
	static const struct strval32 und_macvlan_attrs[] = {
		{ ARG_STR(IFLA_MACVLAN_UNSPEC) },
		{ 10, NULL },
	};

	for (size_t k = 0; k < ARRAY_SIZE(und_macvlan_attrs); k++) {
		CHECK_DATA("macvlan",
			   und_macvlan_attrs[k].val, und_macvlan_attrs[k].str,
			   "IFLA_MACVLAN_???", unknown_msg,
			   { 2, pr_bytes },
			   { 4, pr_bytes });
	}

	static const uint8_t macvlan_addr[] = {
		0xab, 0xac, 0xdb, 0xcd, 0x61, 0x62
	};
	CHECK_DATA("macvlan",
		   IFLA_MACVLAN_MACADDR,
		   "IFLA_MACVLAN_MACADDR", NULL,
		   macvlan_addr,
		   { 2, pr_mac },
		   { 6, pr_mac },
		   { 8, pr_mac });

	struct {
		struct {
			struct nlattr nla;
			uint8_t addr[6];
			uint8_t pad[2];
		} first, second;
	} macvlan_addrs = {
		.first = {
			.nla = { .nla_len = NLA_HDRLEN + 6,
				 .nla_type = IFLA_MACVLAN_MACADDR },
			.addr = { 0xde, 0xad, 0xfa, 0xce, 0xbe, 0xef },
		},
		.second = {
			.nla = { .nla_len = NLA_HDRLEN + 6,
				 .nla_type = IFLA_MACVLAN_MACADDR },
			.addr = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 },
		},
	};
	CHECK_DATA("macvlan",
		   IFLA_MACVLAN_MACADDR_DATA,
		   "IFLA_MACVLAN_MACADDR_DATA", NULL,
		   macvlan_addrs,
		   { 3, pr_bytes },
		   { sizeof(macvlan_addrs.first), pr_macaddr_data },
		   { sizeof(macvlan_addrs), pr_macaddr_data });

	macvlan_addrs.second.nla.nla_type = IFLA_MACVLAN_MAX + 1;
	macaddr_data_unknown = true;
	CHECK_DATA("macvlan",
		   IFLA_MACVLAN_MACADDR_DATA,
		   "IFLA_MACVLAN_MACADDR_DATA", NULL,
		   macvlan_addrs,
		   { sizeof(macvlan_addrs.first) + 2, pr_macaddr_data },
		   { sizeof(macvlan_addrs), pr_macaddr_data });

	static const struct strval32 u32_macvlan_attrs[] = {
		{ ARG_STR(IFLA_MACVLAN_MACADDR_COUNT) },
		{ ARG_STR(IFLA_MACVLAN_BC_QUEUE_LEN) },
		{ ARG_STR(IFLA_MACVLAN_BC_QUEUE_LEN_USED) },
	};

	for (size_t k = 0; k < ARRAY_SIZE(u32_macvlan_attrs); k++) {
		CHECK_DATA("macvlan",
			   u32_macvlan_attrs[k].val, u32_macvlan_attrs[k].str,
			   NULL, u32_val,
			   { 3, pr_bytes },
			   { 4, pr_u32 });
	}

	static const int32_t bc_cutoff = -2;
	CHECK_DATA("macvlan", 9, "IFLA_MACVLAN_BC_CUTOFF", NULL, bc_cutoff,
		   { 3, pr_bytes },
		   { 4, pr_s32 });

	static const struct strval32 macvlan_modes[] = {
		{ ARG_STR(MACVLAN_MODE_PRIVATE) },
		{ ARG_STR(MACVLAN_MODE_VEPA) },
		{ ARG_STR(MACVLAN_MODE_BRIDGE) },
		{ ARG_STR(MACVLAN_MODE_PASSTHRU) },
		{ ARG_STR(MACVLAN_MODE_SOURCE) },
		{ 0, NULL },
		{ 3, NULL },
	};

	set_xlat(macvlan_modes, ARRAY_SIZE(macvlan_modes) - 2,
		 "MACVLAN_MODE_???");
	for (size_t k = 0; k < ARRAY_SIZE(macvlan_modes); k++) {
		CHECK_DATA("macvlan",
			   IFLA_MACVLAN_MODE,
			   "IFLA_MACVLAN_MODE", NULL,
			   macvlan_modes[k].val,
			   { 4, pr_const32 },
			   { 5, pr_const32 });
	}

	static const uint32_t short_macvlan_mode = MACVLAN_MODE_PRIVATE;
	CHECK_DATA("macvlan", IFLA_MACVLAN_MODE, "IFLA_MACVLAN_MODE", NULL,
		   short_macvlan_mode,
		   { 3, pr_bytes },
		   { 4, pr_const32 });

	static const struct strval32 macvlan_flag_names[] = {
		{ ARG_STR(MACVLAN_FLAG_NOPROMISC) },
		{ ARG_STR(MACVLAN_FLAG_NODST) },
	};
	static const uint16_t macvlan_flags[] = {
		0, MACVLAN_FLAG_NOPROMISC, MACVLAN_FLAG_NODST, 3,
		0xfffc, 0xffff
	};

	set_xlat(macvlan_flag_names, ARRAY_SIZE(macvlan_flag_names),
		 "MACVLAN_FLAG_???");
	for (size_t k = 0; k < ARRAY_SIZE(macvlan_flags); k++) {
		CHECK_DATA("macvlan",
			   IFLA_MACVLAN_FLAGS,
			   "IFLA_MACVLAN_FLAGS", NULL,
			   macvlan_flags[k],
			   { 2, pr_flags16 },
			   { 3, pr_flags16 });
	}

	static const uint16_t short_macvlan_flags = MACVLAN_FLAG_NOPROMISC;
	CHECK_DATA("macvlan", IFLA_MACVLAN_FLAGS, "IFLA_MACVLAN_FLAGS", NULL,
		   short_macvlan_flags,
		   { 1, pr_bytes },
		   { 2, pr_flags16 });

	static const struct strval32 macvlan_macaddr_modes[] = {
		{ ARG_STR(MACVLAN_MACADDR_ADD) },
		{ ARG_STR(MACVLAN_MACADDR_DEL) },
		{ ARG_STR(MACVLAN_MACADDR_FLUSH) },
		{ ARG_STR(MACVLAN_MACADDR_SET) },
		{ 4, NULL },
	};

	set_xlat(macvlan_macaddr_modes, ARRAY_SIZE(macvlan_macaddr_modes) - 1,
		 "MACVLAN_MACADDR_???");
	for (size_t k = 0; k < ARRAY_SIZE(macvlan_macaddr_modes); k++) {
		CHECK_DATA("macvlan",
			   IFLA_MACVLAN_MACADDR_MODE,
			   "IFLA_MACVLAN_MACADDR_MODE", NULL,
			   macvlan_macaddr_modes[k].val,
			   { 4, pr_const32 },
			   { 5, pr_const32 });
	}

	static const uint32_t short_macvlan_macaddr_mode = MACVLAN_MACADDR_DEL;
	CHECK_DATA("macvlan",
		   IFLA_MACVLAN_MACADDR_MODE,
		   "IFLA_MACVLAN_MACADDR_MODE", NULL,
		   short_macvlan_macaddr_mode,
		   { 3, pr_bytes },
		   { 4, pr_const32 });

	/* tun attrs */
	static const struct strval32 u8_tun_attrs[] = {
		{ 4, "IFLA_TUN_PI" },
		{ 5, "IFLA_TUN_VNET_HDR" },
		{ 6, "IFLA_TUN_PERSIST" },
		{ 7, "IFLA_TUN_MULTI_QUEUE" },
	};

	for (size_t k = 0; k < ARRAY_SIZE(u8_tun_attrs); k++) {
		CHECK_DATA("tun",
			   u8_tun_attrs[k].val, u8_tun_attrs[k].str, NULL,
			   u8_val,
			   { 0, NULL },
			   { 1, pr_u8 },
			   { 2, pr_u8 });
	}

	static const struct strval32 u32_tun_attrs[] = {
		{ 8, "IFLA_TUN_NUM_QUEUES" },
		{ 9, "IFLA_TUN_NUM_DISABLED_QUEUES" },
	};

	for (size_t k = 0; k < ARRAY_SIZE(u32_tun_attrs); k++) {
		CHECK_DATA("tun",
			   u32_tun_attrs[k].val, u32_tun_attrs[k].str, NULL,
			   u32_val,
			   { 3, pr_bytes },
			   { 4, pr_u32 },
			   { 5, pr_u32 });
	}

	static const struct strval32 und_tun_attrs[] = {
		{ 0,  "IFLA_TUN_UNSPEC" },
		{ 10, NULL },
	};

	for (size_t k = 0; k < ARRAY_SIZE(und_tun_attrs); k++) {
		CHECK_DATA("tun",
			   und_tun_attrs[k].val, und_tun_attrs[k].str,
			   "IFLA_TUN_???", unknown_msg,
			   { 2, pr_bytes },
			   { 4, pr_bytes },
			   { 6, pr_bytes },
			   { 8, pr_bytes },
			   { 10, pr_bytes });
	}

	static const uint32_t minus_one = 0xffffffffU;
	CHECK_DATA("tun", 1, "IFLA_TUN_OWNER", NULL, u32_val,
		   { 3, pr_bytes },
		   { 4, pr_uid },
		   { 5, pr_uid });
	CHECK_DATA("tun", 1, "IFLA_TUN_OWNER", NULL, minus_one,
		   { 3, pr_bytes },
		   { 4, pr_uid },
		   { 5, pr_uid });

	static const struct strval32 tun_types[] = {
		{ 1, "IFF_TUN" },
		{ 2, "IFF_TAP" },
		{ 0, NULL },
		{ 3, NULL },
		{ 0xda, NULL },
	};

	set_xlat(tun_types, ARRAY_SIZE(tun_types) - 3, "IFF_???");
	for (size_t k = 0; k < ARRAY_SIZE(tun_types); k++) {
		const uint8_t tun_type = tun_types[k].val;

		CHECK_DATA("tun", 3, "IFLA_TUN_TYPE", NULL, tun_type,
			   { 0, NULL },
			   { 1, pr_const8 },
			   { 2, pr_const8 });
	}

	/* can xstats: too short, unreadable, and decoded */
	static const uint32_t can_stats_data[] = {
		0xbadc0de0, 0xbadc0de1, 0xbadc0de2, 0xbadc0de3,
		0xbadc0de4, 0xbadc0de5,
	};

	memcpy(li_buf + li_init(info_kind, "can",
				(struct strval32) {
					ARG_STR(IFLA_INFO_XSTATS) },
				NULL, sizeof(can_stats_data)),
	       can_stats_data, sizeof(can_stats_data));
	check_li(fd, nlh0, 1, 0, pr_bytes, sizeof(can_stats_data) - 1);
	check_li(fd, nlh0, 0, 1, pr_addr, 0);
	check_li(fd, nlh0, 0, 0, pr_can_stats, sizeof(can_stats_data));

	/* bridge slave attrs */
	static const struct strval32 und_brport_attrs[] = {
		{ 0, "IFLA_BRPORT_UNSPEC" },
		{ 24, "IFLA_BRPORT_FLUSH" },
		{ 26, "IFLA_BRPORT_PAD" },
		{ 46, NULL },
		{ 2989, NULL },
	};

	for (size_t k = 0; k < ARRAY_SIZE(und_brport_attrs); k++) {
		CHECK_SLAVE_DATA("bridge",
				 und_brport_attrs[k].val,
				 und_brport_attrs[k].str,
				 "IFLA_BRPORT_???", unknown_msg,
				 { 2, pr_bytes },
				 { 4, pr_bytes },
				 { 6, pr_bytes },
				 { 8, pr_bytes },
				 { 10, pr_bytes });
	}

	static const struct strval32 u8_brport_attrs[] = {
		{ ARG_STR(IFLA_BRPORT_STATE) },
		{ ARG_STR(IFLA_BRPORT_MODE) },
		{ ARG_STR(IFLA_BRPORT_GUARD) },
		{ ARG_STR(IFLA_BRPORT_PROTECT) },
		{ ARG_STR(IFLA_BRPORT_FAST_LEAVE) },
		{ ARG_STR(IFLA_BRPORT_LEARNING) },
		{ ARG_STR(IFLA_BRPORT_UNICAST_FLOOD) },
		{ ARG_STR(IFLA_BRPORT_PROXYARP) },
		{ ARG_STR(IFLA_BRPORT_LEARNING_SYNC) },
		{ ARG_STR(IFLA_BRPORT_PROXYARP_WIFI) },
		{ ARG_STR(IFLA_BRPORT_TOPOLOGY_CHANGE_ACK) },
		{ ARG_STR(IFLA_BRPORT_CONFIG_PENDING) },
		{ ARG_STR(IFLA_BRPORT_MULTICAST_ROUTER) },
		{ ARG_STR(IFLA_BRPORT_MCAST_FLOOD) },
		{ ARG_STR(IFLA_BRPORT_MCAST_TO_UCAST) },
		{ ARG_STR(IFLA_BRPORT_VLAN_TUNNEL) },
		{ ARG_STR(IFLA_BRPORT_BCAST_FLOOD) },
		{ ARG_STR(IFLA_BRPORT_NEIGH_SUPPRESS) },
		{ ARG_STR(IFLA_BRPORT_ISOLATED) },
		{ ARG_STR(IFLA_BRPORT_MRP_RING_OPEN) },
		{ ARG_STR(IFLA_BRPORT_MRP_IN_OPEN) },
		{ ARG_STR(IFLA_BRPORT_LOCKED) },
		{ ARG_STR(IFLA_BRPORT_MAB) },
		{ ARG_STR(IFLA_BRPORT_NEIGH_VLAN_SUPPRESS) },
		{ ARG_STR(IFLA_BRPORT_NEIGH_FORWARD_GRAT) },
	};

	for (size_t k = 0; k < ARRAY_SIZE(u8_brport_attrs); k++) {
		CHECK_SLAVE_DATA("bridge",
				 u8_brport_attrs[k].val,
				 u8_brport_attrs[k].str, NULL, u8_val,
				 { 0, NULL },
				 { 1, pr_u8 },
				 { 2, pr_u8 });
	}

	static const struct strval32 u16_brport_attrs[] = {
		{ ARG_STR(IFLA_BRPORT_PRIORITY) },
		{ ARG_STR(IFLA_BRPORT_DESIGNATED_PORT) },
		{ ARG_STR(IFLA_BRPORT_DESIGNATED_COST) },
		{ ARG_STR(IFLA_BRPORT_ID) },
		{ ARG_STR(IFLA_BRPORT_NO) },
	};

	for (size_t k = 0; k < ARRAY_SIZE(u16_brport_attrs); k++) {
		CHECK_SLAVE_DATA("bridge",
				 u16_brport_attrs[k].val,
				 u16_brport_attrs[k].str, NULL, u16_val,
				 { 1, pr_bytes },
				 { 2, pr_u16 },
				 { 3, pr_u16 });
	}

	CHECK_SLAVE_DATA("bridge",
			 IFLA_BRPORT_GROUP_FWD_MASK,
			 "IFLA_BRPORT_GROUP_FWD_MASK", NULL,
			 u16_val,
			 { 1, pr_bytes },
			 { 2, pr_x16 },
			 { 3, pr_x16 });

	static const struct strval32 u32_brport_attrs[] = {
		{  3, "IFLA_BRPORT_COST" },
		{ 37, "IFLA_BRPORT_MCAST_EHT_HOSTS_LIMIT" },
		{ 38, "IFLA_BRPORT_MCAST_EHT_HOSTS_CNT" },
		{ ARG_STR(IFLA_BRPORT_MCAST_N_GROUPS) },
		{ ARG_STR(IFLA_BRPORT_MCAST_MAX_GROUPS) },
		{ ARG_STR(IFLA_BRPORT_BACKUP_NHID) },
	};

	for (size_t k = 0; k < ARRAY_SIZE(u32_brport_attrs); k++) {
		CHECK_SLAVE_DATA("bridge",
				 u32_brport_attrs[k].val,
				 u32_brport_attrs[k].str, NULL, u32_val,
				 { 3, pr_bytes },
				 { 4, pr_u32 },
				 { 5, pr_u32 });
	}

	static const struct strval32 brport_id_attrs[] = {
		{ 13, "IFLA_BRPORT_ROOT_ID" },
		{ 14, "IFLA_BRPORT_BRIDGE_ID" },
	};

	for (size_t k = 0; k < ARRAY_SIZE(brport_id_attrs); k++) {
		CHECK_SLAVE_DATA("bridge",
				 brport_id_attrs[k].val,
				 brport_id_attrs[k].str, NULL, bridge_id,
				 { 7, pr_bytes },
				 { 8, pr_bridge_id },
				 { 9, pr_bridge_id });
	}

	static const struct strval32 c_t_brport_attrs[] = {
		{ 21, "IFLA_BRPORT_MESSAGE_AGE_TIMER" },
		{ 22, "IFLA_BRPORT_FORWARD_DELAY_TIMER" },
		{ 23, "IFLA_BRPORT_HOLD_TIMER" },
	};

	for (size_t k = 0; k < ARRAY_SIZE(c_t_brport_attrs); k++) {
		CHECK_SLAVE_DATA("bridge",
				 c_t_brport_attrs[k].val,
				 c_t_brport_attrs[k].str, NULL, u64_val,
				 { 7, pr_ticks },
				 { 8, pr_ticks },
				 { 9, pr_bytes });
	}

	const uint32_t ifidx_lo = ifindex_lo();

	CHECK_SLAVE_DATA("bridge", 34, "IFLA_BRPORT_BACKUP_PORT", NULL,
			 u32_val,
			 { 3, pr_bytes },
			 { 4, pr_ifindex },
			 { 5, pr_ifindex });
	CHECK_SLAVE_DATA("bridge", 34, "IFLA_BRPORT_BACKUP_PORT", NULL,
			 ifidx_lo,
			 { 3, pr_bytes },
			 { 4, pr_ifindex },
			 { 5, pr_ifindex });

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
