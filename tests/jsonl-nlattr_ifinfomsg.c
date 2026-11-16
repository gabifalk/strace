/*
 * Check JSONL output of IFLA_* netlink attributes of ifinfomsg.
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
# include <linux/if.h>
# include <linux/if_arp.h>
# include <linux/if_link.h>

static const unsigned int hdrlen = sizeof(struct ifinfomsg);

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
init_prop_list_msg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	init_ifinfomsg(nlh, msg_len);

	struct nlattr *nla = NLMSG_ATTR(nlh, hdrlen);
	SET_STRUCT(struct nlattr, nla,
		.nla_len = msg_len - NLMSG_SPACE(hdrlen),
		.nla_type = IFLA_PROP_LIST,
	);
}

static void
print_prop_list_msg(const unsigned int msg_len)
{
	print_ifinfomsg(msg_len);
	jsonl_nlattr_nest_open(msg_len - NLMSG_SPACE(hdrlen),
			       ARG_STR(IFLA_PROP_LIST), NULL);
}

static void
print_u_field(const char *name, uint64_t val, bool first)
{
	jsonl_field_open(name, first);
	jsonl_unsigned(val);
	jsonl_field_close();
}

static void
print_x_field(const char *name, uint64_t val, bool first)
{
	jsonl_field_open(name, first);
	jsonl_hex(val);
	jsonl_field_close();
}

static const char *const stats_names[] = {
	"rx_packets", "tx_packets", "rx_bytes", "tx_bytes",
	"rx_errors", "tx_errors", "rx_dropped", "tx_dropped",
	"multicast", "collisions", "rx_length_errors", "rx_over_errors",
	"rx_crc_errors", "rx_frame_errors", "rx_fifo_errors",
	"rx_missed_errors", "tx_aborted_errors", "tx_carrier_errors",
	"tx_fifo_errors", "tx_heartbeat_errors", "tx_window_errors",
	"rx_compressed", "tx_compressed", "rx_nohandler",
	"rx_otherhost_dropped"
};

/* The first size bytes of a struct rtnl_link_stats. */
static void
print_stats(const struct rtnl_link_stats *const st, const size_t size)
{
	uint32_t vals[sizeof(*st) / sizeof(uint32_t)];

	memcpy(vals, st, sizeof(vals));
	jsonl_struct_open();
	for (size_t i = 0; i < size / sizeof(vals[0]); ++i)
		print_u_field(stats_names[i], vals[i], i == 0);
	jsonl_struct_close();
}

/* The first size bytes of a struct rtnl_link_stats64. */
static void
print_stats64(const struct rtnl_link_stats64 *const st, const size_t size)
{
	uint64_t vals[sizeof(*st) / sizeof(uint64_t)];

	memcpy(vals, st, sizeof(vals));
	jsonl_struct_open();
	for (size_t i = 0; i < size / sizeof(vals[0]); ++i)
		print_u_field(stats_names[i], vals[i], i == 0);
	jsonl_struct_close();
}

static void
print_unterminated_string(const void *p, size_t len)
{
	jsonl_string_value(p, len, true);
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	static const struct rtnl_link_stats st = {
		.rx_packets = 0xabcdefac,
		.tx_packets = 0xbcdacdab,
		.rx_bytes = 0xcdbafaab,
		.tx_bytes = 0xdafabadb,
		.rx_errors = 0xeabcdaeb,
		.tx_errors = 0xfefabeab,
		.rx_dropped = 0xadbafafb,
		.tx_dropped = 0xbdffabda,
		.multicast = 0xcdabdfea,
		.collisions = 0xefadbaeb,
		.rx_length_errors = 0xfabffabd,
		.rx_over_errors = 0xafbafabc,
		.rx_crc_errors = 0xbfdabdad,
		.rx_frame_errors = 0xcfdabfad,
		.rx_fifo_errors = 0xddfdebad,
		.rx_missed_errors = 0xefabdcba,
		.tx_aborted_errors = 0xefdadbfa,
		.tx_carrier_errors = 0xfaefbada,
		.tx_fifo_errors = 0xaebdffab,
		.tx_heartbeat_errors = 0xbadebaaf,
		.tx_window_errors = 0xcdafbada,
		.rx_compressed = 0xdeffadbd,
		.tx_compressed = 0xefdadfab
	};
	const int fd = create_nl_socket(NETLINK_ROUTE);
	void *nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
				   2 * NLA_HDRLEN + MAX(sizeof(st), 20));

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	const unsigned int nla_type = 0xffff & NLA_TYPE_MASK;
	JSONL_NLATTR_(fd, nlh0, hdrlen, init_ifinfomsg, print_ifinfomsg,
		      nla_type, NULL, "IFLA_???",
		      4, pattern, 4,
		      jsonl_bytes_value(pattern, 4, false));

	const int32_t netnsid = 0xacbdabda;
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_ifinfomsg, print_ifinfomsg,
			    IFLA_LINK_NETNSID, pattern, netnsid,
			    jsonl_decimal(netnsid));

	/* struct rtnl_link_stats with and without rx_nohandler */
	const unsigned int sizeof_stats =
		offsetofend(struct rtnl_link_stats, tx_compressed);
	JSONL_NLATTR_OBJECT_MINSZ(fd, nlh0, hdrlen,
				  init_ifinfomsg, print_ifinfomsg,
				  IFLA_STATS, pattern, st, sizeof_stats,
				  print_stats(&st, sizeof(st)));

	JSONL_NLATTR(fd, nlh0, hdrlen, init_ifinfomsg, print_ifinfomsg,
		     IFLA_STATS, sizeof_stats, &st, sizeof_stats,
		     print_stats(&st, sizeof_stats));

	static const struct rtnl_link_ifmap map = {
		.mem_start = 0xadcbefedefbcdedb,
		.mem_end = 0xefcbeabdecdcdefa,
		.base_addr = 0xaddbeabdfaacdbae,
		.irq = 0xefaf,
		.dma = 0xab,
		.port = 0xcd
	};
	const unsigned int sizeof_ifmap =
		offsetofend(struct rtnl_link_ifmap, port);
	const unsigned int plen = MIN(sizeof_ifmap - 1, DEFAULT_STRLEN);
	/* len < sizeof_ifmap */
	JSONL_NLATTR(fd, nlh0, hdrlen, init_ifinfomsg, print_ifinfomsg,
		     IFLA_MAP, plen, pattern, plen,
		     jsonl_bytes_value(pattern, plen, false));

	/* short read of sizeof_ifmap */
	JSONL_NLATTR(fd, nlh0, hdrlen, init_ifinfomsg, print_ifinfomsg,
		     IFLA_MAP, sizeof_ifmap, &map, sizeof_ifmap - 1,
		     jsonl_addr(jsonl_nl_data));

	/* sizeof_ifmap */
	JSONL_NLATTR(fd, nlh0, hdrlen, init_ifinfomsg, print_ifinfomsg,
		     IFLA_MAP, sizeof_ifmap, &map, sizeof_ifmap,
		     jsonl_struct_open();
		     print_x_field("mem_start", map.mem_start, true);
		     print_x_field("mem_end", map.mem_end, false);
		     print_x_field("base_addr", map.base_addr, false);
		     print_u_field("irq", map.irq, false);
		     print_u_field("dma", map.dma, false);
		     print_u_field("port", map.port, false);
		     jsonl_struct_close());

	/* struct rtnl_link_stats64 of three historical sizes */
	static const struct rtnl_link_stats64 st64 = {
		.rx_packets = 0xadcbefedefbcdedb,
		.tx_packets = 0xbdabdedabdcdeabd,
		.rx_bytes = 0xcdbaefbaeadfabec,
		.tx_bytes = 0xdbaedbafabbeacdb,
		.rx_errors = 0xefabfdaefabaefab,
		.tx_errors = 0xfaebfabfabbaeabf,
		.rx_dropped = 0xacdbaedbadbabeba,
		.tx_dropped = 0xbcdeffebdabeadbe,
		.multicast = 0xeeffbaeabaeffabe,
		.collisions = 0xffbaefcefbafacef,
		.rx_length_errors = 0xaabbdeabceffdecb,
		.rx_over_errors = 0xbbdcdadebadeaeed,
		.rx_crc_errors = 0xccdeabecefaedbef,
		.rx_frame_errors = 0xddbedaedebcedaef,
		.rx_fifo_errors = 0xeffbadefafdaeaab,
		.rx_missed_errors = 0xfefaebccceadeecd,
		.tx_aborted_errors = 0xabcdadefcdadef,
		.tx_carrier_errors = 0xbccdafaeeaaefe,
		.tx_fifo_errors = 0xcddefdbedeadce,
		.tx_heartbeat_errors = 0xedaededdadcdea,
		.tx_window_errors = 0xfdacdeaccedcda,
		.rx_compressed = 0xacdbbcacdbccef,
		.tx_compressed = 0xbcdadefcdedfea,
		.rx_nohandler = 0xcbdbacbfbafffd,
		.rx_otherhost_dropped = 0xbefdafcfeeadcbfb
	};
	JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen, init_ifinfomsg, print_ifinfomsg,
			    IFLA_STATS64, pattern, st64,
			    print_stats64(&st64, sizeof(st64)));

	const unsigned int stats64_rx_nohandler_size =
		offsetofend(struct rtnl_link_stats64, rx_nohandler);
	JSONL_NLATTR(fd, nlh0, hdrlen, init_ifinfomsg, print_ifinfomsg,
		     IFLA_STATS64, stats64_rx_nohandler_size,
		     &st64, stats64_rx_nohandler_size,
		     print_stats64(&st64, stats64_rx_nohandler_size));

	const unsigned int stats64_tx_compressed_size =
		offsetofend(struct rtnl_link_stats64, tx_compressed);
	JSONL_NLATTR(fd, nlh0, hdrlen, init_ifinfomsg, print_ifinfomsg,
		     IFLA_STATS64, stats64_tx_compressed_size,
		     &st64, stats64_tx_compressed_size,
		     print_stats64(&st64, stats64_tx_compressed_size));

	/* Nested attributes without a payload */
	struct nlattr nla = {
		.nla_len = sizeof(nla),
		.nla_type = IFLA_INFO_KIND,
	};
	JSONL_NLATTR(fd, nlh0, hdrlen, init_ifinfomsg, print_ifinfomsg,
		     IFLA_LINKINFO, sizeof(nla), &nla, sizeof(nla),
		     jsonl_nlattr_hdr(nla.nla_len, ARG_STR(IFLA_INFO_KIND),
				      NULL));

	nla.nla_type = IFLA_VF_PORT;
	JSONL_NLATTR(fd, nlh0, hdrlen, init_ifinfomsg, print_ifinfomsg,
		     IFLA_VF_PORTS, sizeof(nla), &nla, sizeof(nla),
		     jsonl_nlattr_hdr(nla.nla_len, ARG_STR(IFLA_VF_PORT),
				      NULL));

	/* IFLA_EXT_MASK */
	static const struct strval32 rtext_filters[] = {
		{ ARG_STR(RTEXT_FILTER_VF) },
		{ ARG_STR(RTEXT_FILTER_BRVLAN) },
		{ ARG_STR(RTEXT_FILTER_BRVLAN_COMPRESSED) },
		{ ARG_STR(RTEXT_FILTER_SKIP_STATS) },
		{ ARG_STR(RTEXT_FILTER_MRP) },
		{ ARG_STR(RTEXT_FILTER_CFM_CONFIG) },
		{ ARG_STR(RTEXT_FILTER_CFM_STATUS) },
		{ ARG_STR(RTEXT_FILTER_MST) },
	};
	static const uint32_t ifla_ext_masks[] = {
		0, RTEXT_FILTER_VF, 0xdeface00, 0xdeadfeed
	};
	for (size_t i = 0; i < ARRAY_SIZE(ifla_ext_masks); i++) {
		JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen,
				    init_ifinfomsg, print_ifinfomsg,
				    IFLA_EXT_MASK, pattern, ifla_ext_masks[i],
				    jsonl_flags(ifla_ext_masks[i],
						rtext_filters,
						ARRAY_SIZE(rtext_filters),
						"RTEXT_FILTER_???"));
	}

	/* IFLA_EVENT */
	static const struct strval32 ifla_events[] = {
		{ ARG_STR(IFLA_EVENT_NONE) },
		{ ARG_STR(IFLA_EVENT_BONDING_OPTIONS) },
		{ 0x7, NULL },
		{ 0xdeadfeed, NULL },
	};
	for (size_t i = 0; i < ARRAY_SIZE(ifla_events); i++) {
		JSONL_NLATTR_OBJECT(fd, nlh0, hdrlen,
				    init_ifinfomsg, print_ifinfomsg,
				    IFLA_EVENT, pattern, ifla_events[i].val,
				    jsonl_const(ifla_events[i].val, true,
						ifla_events[i].str,
						"IFLA_EVENT_???"));
	}

	/* IFLA_PROP_LIST */
	struct {
		char p1[20];
	} buf;
	fill_memory(&buf, sizeof(buf));
	JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen,
				       init_prop_list_msg, print_prop_list_msg,
				       IFLA_ALT_IFNAME, "IFLA_ALT_IFNAME", NULL,
				       pattern, buf,
				       print_unterminated_string, 1,
				       jsonl_string_value(&buf, sizeof(buf),
							  true));

	/* IFLA_ALT_IFNAME, IFLA_PARENT_DEV_NAME, IFLA_PARENT_DEV_BUS_NAME */
	static const char str[] = "OH HAI THAR\r\n\t\377\0\v\x7e";
	static const struct strval32 attrs[] = {
		{ ARG_STR(IFLA_ALT_IFNAME) },
		{ ARG_STR(IFLA_PARENT_DEV_NAME) },
		{ ARG_STR(IFLA_PARENT_DEV_BUS_NAME) },
	};
	for (size_t i = 0; i < ARRAY_SIZE(attrs); i++) {
		/* The trailing NUL is not a part of the string value. */
		JSONL_NLATTR_(fd, nlh0, hdrlen,
			      init_ifinfomsg, print_ifinfomsg,
			      attrs[i].val, attrs[i].str, NULL,
			      sizeof(str), str, sizeof(str),
			      jsonl_string_value(str, sizeof(str) - 1, false));

		JSONL_NLATTR_(fd, nlh0, hdrlen,
			      init_ifinfomsg, print_ifinfomsg,
			      attrs[i].val, attrs[i].str, NULL,
			      sizeof(str) - 1, str, sizeof(str) - 1,
			      jsonl_string_value(str, sizeof(str) - 1, true));
	}

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
