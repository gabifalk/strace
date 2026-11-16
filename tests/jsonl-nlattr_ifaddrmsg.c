/*
 * Check JSONL output of IFA_* netlink attributes of ifaddrmsg.
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
# include <netinet/in.h>
# include <linux/if_addr.h>

static const unsigned int hdrlen = sizeof(struct ifaddrmsg);

static uint32_t idx;
static uint8_t ifa_family;
static const char *ifa_family_str;

static const struct strval32 ifa_f_flags[] = {
	{ ARG_STR(IFA_F_SECONDARY) },
	{ ARG_STR(IFA_F_NODAD) },
	{ ARG_STR(IFA_F_OPTIMISTIC) },
	{ ARG_STR(IFA_F_DADFAILED) },
	{ ARG_STR(IFA_F_HOMEADDRESS) },
	{ ARG_STR(IFA_F_DEPRECATED) },
	{ ARG_STR(IFA_F_TENTATIVE) },
	{ ARG_STR(IFA_F_PERMANENT) },
	{ ARG_STR(IFA_F_MANAGETEMPADDR) },
	{ ARG_STR(IFA_F_NOPREFIXROUTE) },
	{ ARG_STR(IFA_F_MCAUTOJOIN) },
	{ ARG_STR(IFA_F_STABLE_PRIVACY) },
};

/* The array sizes of ifa_flags and ifa_scopes are chosen to be coprime. */
static const uint8_t ifa_flags[] = { 0, IFA_F_SECONDARY, 0xff };
static const struct strval32 ifa_scopes[] = {
	{ ARG_STR(RT_SCOPE_UNIVERSE) },
	{ 0x1, NULL },
	{ 0x2, NULL },
	{ 0x7f, NULL },
	{ 0xc7, NULL },
	{ ARG_STR(RT_SCOPE_SITE) },
	{ 0xc9, NULL },
	{ 0xfc, NULL },
	{ ARG_STR(RT_SCOPE_LINK) },
	{ ARG_STR(RT_SCOPE_HOST) },
	{ ARG_STR(RT_SCOPE_NOWHERE) },
};

static void
init_ifaddrmsg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	idx++;

	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = RTM_GETADDR,
		.nlmsg_flags = NLM_F_DUMP
	);

	struct ifaddrmsg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct ifaddrmsg, msg,
		.ifa_family = ifa_family,
		.ifa_flags = ARR_ITEM(ifa_flags, idx),
		.ifa_scope = ARR_ITEM(ifa_scopes, idx).val,
		.ifa_index = ifindex_lo()
	);
}

static void
print_ifaddrmsg(const unsigned int msg_len)
{
	static const struct strval32 nlmsg_flags[] = {
		{ ARG_STR(NLM_F_DUMP) }
	};

	printf("{");
	jsonl_nlmsghdr_vals(msg_len, ARG_STR(RTM_GETADDR), NULL,
			    NLM_F_DUMP, nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	jsonl_struct_open();
	jsonl_field_open("ifa_family", true);
	jsonl_const(ifa_family, true, ifa_family_str, NULL);
	jsonl_field_close();
	jsonl_field_open("ifa_prefixlen", false);
	jsonl_unsigned(0);
	jsonl_field_close();
	jsonl_field_open("ifa_flags", false);
	jsonl_flags(ARR_ITEM(ifa_flags, idx), ifa_f_flags,
		    ARRAY_SIZE(ifa_f_flags), "IFA_F_???");
	jsonl_field_close();
	jsonl_field_open("ifa_scope", false);
	jsonl_const(ARR_ITEM(ifa_scopes, idx).val, true,
		    ARR_ITEM(ifa_scopes, idx).str, NULL);
	jsonl_field_close();
	jsonl_field_open("ifa_index", false);
	jsonl_ifindex(ifindex_lo(), "lo");
	jsonl_field_close();
	jsonl_struct_close();
	printf("}");
}

static void
print_lifetime_field(const char *name, uint32_t val, bool first)
{
	jsonl_field_open(name, first);
	jsonl_const(val, false, val == -1U ? "INFINITY_LIFE_TIME" : NULL, NULL);
	jsonl_field_close();
}

static void
print_ticks_field(const char *name, uint32_t val)
{
	jsonl_field_open(name, false);
	jsonl_ticks(val);
	jsonl_field_close();
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	static const uint32_t ifa_flags_ext[] = {
		0, IFA_F_SECONDARY, 0x1000, 0xffffffff
	};
	static const char address4[] = "12.34.56.78";
	static const char address6[] = "12:34:56:78:90:ab:cd:ef";
	static const struct ifa_cacheinfo cis[] = {
		{
			.ifa_prefered = 0,
			.ifa_valid = 0,
			.cstamp = 0,
			.tstamp = 0,
		},
		{
			.ifa_prefered = 1,
			.ifa_valid = 2,
			.cstamp = 300,
			.tstamp = 400,
		},
		{
			.ifa_prefered = 0xabcdefac,
			.ifa_valid = 0xbcdadbca,
			.cstamp = 0xcdabedba,
			.tstamp = 0xdebabdac,
		},
		{
			.ifa_prefered = 0xffffffff,
			.ifa_valid = 0xffffffff,
			.cstamp = 0xffffffff,
			.tstamp = 0xffffffff,
		},
	};

	struct in_addr a4;
	struct in6_addr a6;

	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
				   NLA_HDRLEN + MAX(sizeof(cis[0]),
						    sizeof(a6)));

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	if (!inet_pton(AF_INET, address4, &a4))
		perror_msg_and_skip("inet_pton(AF_INET, address4)");

	if (!inet_pton(AF_INET6, address6, &a6))
		perror_msg_and_skip("inet_pton(AF_INET6, address6)");

	const struct {
		uint8_t af;
		const char *af_str;
		const void *addr;
		size_t addr_sz;
		const char *addr_str;
	} afs[] = {
		{ ARG_STR(AF_UNSPEC), NULL, 0, NULL },
		{ ARG_STR(AF_INET), &a4, sizeof(a4), address4 },
		{ ARG_STR(AF_INET6), &a6, sizeof(a6), address6 },
	};

	for (size_t af_idx = 0; af_idx < ARRAY_SIZE(afs); af_idx++) {
		ifa_family = afs[af_idx].af;
		ifa_family_str = afs[af_idx].af_str;

		/* Unimplemented attribute type */
		for (size_t j = 1; j < DEFAULT_STRLEN + 2; j++) {
			JSONL_NLATTR(fd, nlh0, hdrlen,
				     init_ifaddrmsg, print_ifaddrmsg,
				     IFA_UNSPEC, j, pattern, j,
				     jsonl_bytes_value(pattern,
						       MIN(j, DEFAULT_STRLEN),
						       j > DEFAULT_STRLEN));
		}

		/* Unknown attribute types */
		JSONL_NLATTR_(fd, nlh0, hdrlen,
			      init_ifaddrmsg, print_ifaddrmsg,
			      13, NULL, "IFA_???",
			      4, pattern, 4,
			      jsonl_bytes_value(pattern, 4, false));

		const unsigned int nla_type = 0xffff & NLA_TYPE_MASK;
		JSONL_NLATTR_(fd, nlh0, hdrlen,
			      init_ifaddrmsg, print_ifaddrmsg,
			      nla_type, NULL, "IFA_???",
			      4, pattern, 4,
			      jsonl_bytes_value(pattern, 4, false));

		/* Addresses */
		static const struct strval32 addr_attrs[] = {
			{ ARG_STR(IFA_ADDRESS) },
			{ ARG_STR(IFA_LOCAL) },
			{ ARG_STR(IFA_BROADCAST) },
			{ ARG_STR(IFA_ANYCAST) },
			{ ARG_STR(IFA_MULTICAST) },
		};
		for (size_t i = 0; i < ARRAY_SIZE(addr_attrs); i++) {
			if (afs[af_idx].addr) {
				JSONL_NESTED_NLATTR_OBJPTR_EX_(fd, nlh0, hdrlen,
					init_ifaddrmsg, print_ifaddrmsg,
					addr_attrs[i].val,
					addr_attrs[i].str, NULL,
					pattern, afs[af_idx].addr,
					afs[af_idx].addr_sz,
					afs[af_idx].addr_sz,
					jsonl_nl_bytes, 0,
					jsonl_ip_addr(ifa_family,
						      ifa_family_str,
						      afs[af_idx].addr_str));
			} else {
				JSONL_NLATTR_(fd, nlh0, hdrlen,
					      init_ifaddrmsg, print_ifaddrmsg,
					      addr_attrs[i].val,
					      addr_attrs[i].str, NULL,
					      4, pattern, 4,
					      jsonl_bytes_value(pattern, 4,
								false));
			}
		}

		/* An unterminated string, also cut at the string limit */
		JSONL_NLATTR(fd, nlh0, hdrlen,
			     init_ifaddrmsg, print_ifaddrmsg,
			     IFA_LABEL, 8, pattern, 8,
			     jsonl_string_value(pattern, 8, true));
		JSONL_NLATTR(fd, nlh0, hdrlen,
			     init_ifaddrmsg, print_ifaddrmsg,
			     IFA_LABEL, 40, pattern, 40,
			     jsonl_string_value(pattern, DEFAULT_STRLEN, true));

		/* struct ifa_cacheinfo */
		for (size_t i = 0; i < ARRAY_SIZE(cis); i++) {
			JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen,
				init_ifaddrmsg, print_ifaddrmsg,
				IFA_CACHEINFO, pattern, cis[i],
				jsonl_struct_open();
				print_lifetime_field("ifa_prefered",
						     cis[i].ifa_prefered, true);
				print_lifetime_field("ifa_valid",
						     cis[i].ifa_valid, false);
				print_ticks_field("cstamp", cis[i].cstamp);
				print_ticks_field("tstamp", cis[i].tstamp);
				jsonl_struct_close());
		}

		/* IFA_F_* */
		for (size_t i = 0; i < ARRAY_SIZE(ifa_flags_ext); i++) {
			JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen,
				init_ifaddrmsg, print_ifaddrmsg,
				IFA_FLAGS, pattern, ifa_flags_ext[i],
				jsonl_flags(ifa_flags_ext[i], ifa_f_flags,
					    ARRAY_SIZE(ifa_f_flags),
					    "IFA_F_???"));
		}

		/* IFAPROT_* */
		static const struct strval32 protos[] = {
			{ ARG_STR(IFAPROT_UNSPEC) },
			{ ARG_STR(IFAPROT_KERNEL_LO) },
			{ ARG_STR(IFAPROT_KERNEL_LL) },
			{ 0x4, NULL },
			{ 0x5, NULL },
			{ 0xff, NULL },
		};
		for (size_t i = 0; i < ARRAY_SIZE(protos); i++) {
			const uint8_t proto = protos[i].val;

			JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen,
				init_ifaddrmsg, print_ifaddrmsg,
				IFA_PROTO, pattern, proto,
				jsonl_const(proto, true, protos[i].str,
					    "IFAPROT_???"));
		}

		/* u32 */
		JSONL_NLATTR_VAL(u32, fd, nlh0, hdrlen,
				 init_ifaddrmsg, print_ifaddrmsg,
				 IFA_RT_PRIORITY, pattern, 0);
		JSONL_NLATTR_VAL(u32, fd, nlh0, hdrlen,
				 init_ifaddrmsg, print_ifaddrmsg,
				 IFA_MC_USERS, pattern, 0);

		/* s32 */
		JSONL_NLATTR_VAL(s32, fd, nlh0, hdrlen,
				 init_ifaddrmsg, print_ifaddrmsg,
				 IFA_TARGET_NETNSID, pattern, 0);
	}

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
