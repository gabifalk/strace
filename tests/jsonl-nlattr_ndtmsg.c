/*
 * Check JSONL output of NDTA_* netlink attributes of ndtmsg.
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

# include <linux/neighbour.h>

static void
init_ndtmsg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = RTM_GETNEIGHTBL,
		.nlmsg_flags = NLM_F_DUMP
	);

	struct ndtmsg *const msg = NLMSG_DATA(nlh);
	SET_STRUCT(struct ndtmsg, msg,
		.ndtm_family = AF_NETLINK
	);
}

static void
print_ndtmsg(const unsigned int msg_len)
{
	jsonl_nlmsghdr_dump_open(msg_len, ARG_STR(RTM_GETNEIGHTBL));
	jsonl_struct_open();
	jsonl_field_const("ndtm_family", true, AF_NETLINK, "AF_NETLINK", NULL);
	jsonl_struct_close();
	printf("}");
}

# define PRINT_U_FIELD(obj_, field_, first_)	\
	jsonl_field_unsigned(#field_, (first_), (obj_).field_)

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_ROUTE);
	const unsigned int hdrlen = sizeof(struct ndtmsg);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen), NLA_HDRLEN + 11 * 8);

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	const unsigned int nla_type = 0xffff & NLA_TYPE_MASK;
	JSONL_NLATTR_(fd, nlh0, hdrlen, init_ndtmsg, print_ndtmsg,
		      nla_type, NULL, "NDTA_???",
		      4, pattern, 4,
		      jsonl_bytes_value(pattern, 4, false));

	static const struct ndt_config ndtc = {
		.ndtc_key_len = 0xabcd,
		.ndtc_entry_size = 0xbcda,
		.ndtc_entries = 0xcdabedad,
		.ndtc_last_flush = 0xdebaedba,
		.ndtc_last_rand = 0xedadedab,
		.ndtc_hash_rnd = 0xfeadedaf,
		.ndtc_hash_mask = 0xadbcdead,
		.ndtc_hash_chain_gc = 0xbdaedacd,
		.ndtc_proxy_qlen = 0xcdeaedab
	};
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_ndtmsg, print_ndtmsg,
			    NDTA_CONFIG, pattern, ndtc,
			    jsonl_struct_open();
			    PRINT_U_FIELD(ndtc, ndtc_key_len, true);
			    PRINT_U_FIELD(ndtc, ndtc_entry_size, false);
			    PRINT_U_FIELD(ndtc, ndtc_entries, false);
			    PRINT_U_FIELD(ndtc, ndtc_last_flush, false);
			    PRINT_U_FIELD(ndtc, ndtc_last_rand, false);
			    PRINT_U_FIELD(ndtc, ndtc_hash_rnd, false);
			    jsonl_field_open("ndtc_hash_mask", false);
			    jsonl_hex(ndtc.ndtc_hash_mask);
			    jsonl_field_close();
			    PRINT_U_FIELD(ndtc, ndtc_hash_chain_gc, false);
			    PRINT_U_FIELD(ndtc, ndtc_proxy_qlen, false);
			    jsonl_struct_close());

	/* A nested attribute that has no payload is not an array. */
	static const struct nlattr nla = {
		.nla_len = sizeof(nla),
		.nla_type = NDTPA_IFINDEX
	};
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_ndtmsg, print_ndtmsg,
			    NDTA_PARMS, pattern, nla,
			    jsonl_nlattr_hdr(nla.nla_len,
					     ARG_STR(NDTPA_IFINDEX), NULL));

	static const struct ndt_stats ndtst = {
		.ndts_allocs		= 0xabcdedabedadedfa,
		.ndts_destroys		= 0xbcdefabefacdbaad,
		.ndts_hash_grows	= 0xcdbadefacdcbaede,
		.ndts_res_failed	= 0xdedbaecfdbcadcfe,
		.ndts_lookups		= 0xedfafdedbdadedec,
		.ndts_hits		= 0xfebdeadebcddeade,
		.ndts_rcv_probes_mcast	= 0xadebfeadecddeafe,
		.ndts_rcv_probes_ucast	= 0xbcdefeacdadecdfe,
		.ndts_periodic_gc_runs	= 0xedffeadedeffbecc,
		.ndts_forced_gc_runs	= 0xfeefefeabedeedcd,
		.ndts_table_fulls	= 0xadebfefaecdfeade
	};
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_ndtmsg, print_ndtmsg,
			    NDTA_STATS, pattern, ndtst,
			    jsonl_struct_open();
			    PRINT_U_FIELD(ndtst, ndts_allocs, true);
			    PRINT_U_FIELD(ndtst, ndts_destroys, false);
			    PRINT_U_FIELD(ndtst, ndts_hash_grows, false);
			    PRINT_U_FIELD(ndtst, ndts_res_failed, false);
			    PRINT_U_FIELD(ndtst, ndts_lookups, false);
			    PRINT_U_FIELD(ndtst, ndts_hits, false);
			    PRINT_U_FIELD(ndtst, ndts_rcv_probes_mcast, false);
			    PRINT_U_FIELD(ndtst, ndts_rcv_probes_ucast, false);
			    PRINT_U_FIELD(ndtst, ndts_periodic_gc_runs, false);
			    PRINT_U_FIELD(ndtst, ndts_forced_gc_runs, false);
			    PRINT_U_FIELD(ndtst, ndts_table_fulls, false);
			    jsonl_struct_close());

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
