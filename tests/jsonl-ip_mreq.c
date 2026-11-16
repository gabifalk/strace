/*
 * Check JSONL output of struct ip_mreq and struct ipv6_mreq
 * socket options.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"
#include <netinet/in.h>

#if defined __NR_setsockopt \
 && defined IP_ADD_MEMBERSHIP && defined IPV6_ADD_MEMBERSHIP \
 && defined IPV6_JOIN_ANYCAST

# include "jsonl.h"

# include <limits.h>
# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/socket.h>
# include <arpa/inet.h>
# include <net/if.h>

# define multi4addr	"224.0.0.3"
# define multi6addr	"ff01::c"
# define interface	"127.0.0.1"

static bool merged;
static unsigned int lo_ifindex;

static void
jsonl_ip_mreq(void)
{
	jsonl_struct_open();
	jsonl_field_open("imr_multiaddr", true);
	jsonl_ip_addr(ARG_STR(AF_INET), multi4addr);
	jsonl_field_close();
	jsonl_field_open("imr_interface", false);
	jsonl_ip_addr(ARG_STR(AF_INET), interface);
	jsonl_field_close();
	jsonl_struct_close();
}

static void
jsonl_ipv6_mreq(void)
{
	jsonl_struct_open();
	jsonl_field_open("ipv6mr_multiaddr", true);
	jsonl_ip_addr(ARG_STR(AF_INET6), multi6addr);
	jsonl_field_close();
	jsonl_field_ifindex("ipv6mr_interface", false, lo_ifindex, "lo");
	jsonl_struct_close();
}

/* The option value is printed as a pointer when print_val is NULL. */
static void
test_opt(int level, const char *level_str, int name, const char *name_str,
	 const void *val, int len, void (*print_val)(void))
{
	const char *side = merged ? "entry" : NULL;
	long rc = syscall(__NR_setsockopt, 0, level, name, val, len);
	int saved_errno = errno;

	if (merged)
		jsonl_syscall_merged_open("setsockopt", __NR_setsockopt);
	else
		jsonl_syscall_open("setsockopt", __NR_setsockopt, true);

	jsonl_arg_open(0, "sockfd", side);
	jsonl_fd(0, NULL, false);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(1, "level", side);
	jsonl_const(level, true, level_str, NULL);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(2, "optname", side);
	jsonl_const(name, true, name_str, NULL);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(3, "optval", side);
	if (print_val)
		print_val();
	else
		jsonl_addr(val);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(4, "optlen", side);
	jsonl_decimal(len);
	jsonl_arg_close(side);

	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("setsockopt", __NR_setsockopt, false);
	}
	printf("], ");
	errno = saved_errno;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_PTR(struct ip_mreq, m4);
	TAIL_ALLOC_OBJECT_CONST_PTR(struct ipv6_mreq, m6);

	inet_pton(AF_INET, multi4addr, &m4->imr_multiaddr);
	inet_pton(AF_INET, interface, &m4->imr_interface);
	inet_pton(AF_INET6, multi6addr, &m6->ipv6mr_multiaddr);

	lo_ifindex = ifindex_lo();
	if (!lo_ifindex)
		perror_msg_and_skip("lo");
	m6->ipv6mr_interface = lo_ifindex;

	(void) close(0);
	if (socket(AF_INET, SOCK_DGRAM, 0))
		perror_msg_and_skip("socket");

	const struct {
		const int level;
		const char *const str_level;
		const int name;
		const char *str_name;
		const void *const val;
		unsigned int size;
		void (*const print_val)(void);
	} opts[] = {
		{
			ARG_STR(SOL_IP), ARG_STR(IP_ADD_MEMBERSHIP),
			m4, sizeof(*m4), jsonl_ip_mreq
		},
		{
			ARG_STR(SOL_IP), ARG_STR(IP_DROP_MEMBERSHIP),
			m4, sizeof(*m4), jsonl_ip_mreq
		},
		{
			ARG_STR(SOL_IPV6), ARG_STR(IPV6_ADD_MEMBERSHIP),
			m6, sizeof(*m6), jsonl_ipv6_mreq
		},
		{
			ARG_STR(SOL_IPV6), ARG_STR(IPV6_DROP_MEMBERSHIP),
			m6, sizeof(*m6), jsonl_ipv6_mreq
		},
		{
			ARG_STR(SOL_IPV6), ARG_STR(IPV6_JOIN_ANYCAST),
			m6, sizeof(*m6), jsonl_ipv6_mreq
		},
		{
			ARG_STR(SOL_IPV6), ARG_STR(IPV6_LEAVE_ANYCAST),
			m6, sizeof(*m6), jsonl_ipv6_mreq
		}
	};

	for (unsigned int i = 0; i < ARRAY_SIZE(opts); ++i) {
		/* optlen < 0, EINVAL */
		test_opt(opts[i].level, opts[i].str_level,
			 opts[i].name, opts[i].str_name,
			 opts[i].val, -1, NULL);

		/* optlen < sizeof(struct), EINVAL */
		test_opt(opts[i].level, opts[i].str_level,
			 opts[i].name, opts[i].str_name,
			 opts[i].val, opts[i].size - 1, NULL);

		/* optval EFAULT */
		test_opt(opts[i].level, opts[i].str_level,
			 opts[i].name, opts[i].str_name,
			 opts[i].val + 1, opts[i].size, NULL);

		/* classic */
		test_opt(opts[i].level, opts[i].str_level,
			 opts[i].name, opts[i].str_name,
			 opts[i].val, opts[i].size, opts[i].print_val);

		/* optlen > sizeof(struct), shortened */
		test_opt(opts[i].level, opts[i].str_level,
			 opts[i].name, opts[i].str_name,
			 opts[i].val, INT_MAX, opts[i].print_val);
	}

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_setsockopt && IP_ADD_MEMBERSHIP"
		    " && IPV6_ADD_MEMBERSHIP && IPV6_JOIN_ANYCAST")

#endif
