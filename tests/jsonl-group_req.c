/*
 * Check JSONL output of MCAST_JOIN_GROUP and MCAST_LEAVE_GROUP
 * socket options.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"
#include <net/if.h>
#include <netinet/in.h>

#if defined __NR_setsockopt \
 && defined MCAST_JOIN_GROUP && defined MCAST_LEAVE_GROUP

# include "jsonl.h"

# include <limits.h>
# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/socket.h>
# include <arpa/inet.h>

# define multi4addr	"224.0.0.3"
# define multi6addr	"ff01::c"

static bool merged;
static struct group_req *greq4;
static struct group_req *greq6;

static void
jsonl_gr_interface(const struct group_req *greq)
{
	jsonl_struct_open();
	jsonl_field_ifindex("gr_interface", true, greq->gr_interface, "lo");
}

static void
jsonl_port(uint16_t port)
{
	printf(", \"port\": {");
	jsonl_unsigned(ntohs(port));
	jsonl_byteorder_big(sizeof(port));
	printf("}");
}

static void
jsonl_group_req4(void)
{
	struct sockaddr_in sin;

	memcpy(&sin, &greq4->gr_group, sizeof(sin));

	jsonl_gr_interface(greq4);
	jsonl_field_open("gr_group", false);
	printf("\"type\": \"sockaddr\", \"family\": {");
	jsonl_const(AF_INET, true, "AF_INET", NULL);
	printf("}, \"fields\": {\"addr\": \"%s\"", multi4addr);
	jsonl_port(sin.sin_port);
	printf("}");
	jsonl_field_close();
	jsonl_struct_close();
}

static void
jsonl_group_req6(void)
{
	struct sockaddr_in6 sin6;

	memcpy(&sin6, &greq6->gr_group, sizeof(sin6));

	jsonl_gr_interface(greq6);
	jsonl_field_open("gr_group", false);
	printf("\"type\": \"sockaddr\", \"family\": {");
	jsonl_const(AF_INET6, true, "AF_INET6", NULL);
	printf("}, \"fields\": {\"addr\": \"%s\"", multi6addr);
	jsonl_port(sin6.sin6_port);
	printf(", \"flowinfo\": {");
	jsonl_unsigned(ntohl(sin6.sin6_flowinfo));
	jsonl_byteorder_big(sizeof(sin6.sin6_flowinfo));
	printf("}, \"scope_id\": \"%u\"}", sin6.sin6_scope_id);
	jsonl_field_close();
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

	greq4 = tail_alloc(sizeof(*greq4));
	greq6 = tail_alloc(sizeof(*greq6));

	greq6->gr_interface = greq4->gr_interface = ifindex_lo();
	if (!greq4->gr_interface)
		perror_msg_and_skip("lo");

	greq4->gr_group.ss_family = AF_INET;
	union {
		struct sockaddr_in *s;
		typeof(greq4->gr_group) *g;
	} ug4 = { .g = &greq4->gr_group };
	inet_pton(AF_INET, multi4addr, &ug4.s->sin_addr);

	greq6->gr_group.ss_family = AF_INET6;
	union {
		struct sockaddr_in6 *s;
		typeof(greq6->gr_group) *g;
	} ug6 = { .g = &greq6->gr_group };
	inet_pton(AF_INET6, multi6addr, &ug6.s->sin6_addr);

	(void) close(0);
	if (socket(AF_INET, SOCK_DGRAM, 0))
		perror_msg_and_skip("socket");

	const struct {
		const int level;
		const char *const str_level;
		const int name;
		const char *const str_name;
		const struct group_req *const val;
		void (*const print_val)(void);
	} opts[] = {
		{
			ARG_STR(SOL_IP), ARG_STR(MCAST_JOIN_GROUP),
			greq4, jsonl_group_req4
		},
		{
			ARG_STR(SOL_IP), ARG_STR(MCAST_LEAVE_GROUP),
			greq4, jsonl_group_req4
		},
		{
			ARG_STR(SOL_IPV6), ARG_STR(MCAST_JOIN_GROUP),
			greq6, jsonl_group_req6
		},
		{
			ARG_STR(SOL_IPV6), ARG_STR(MCAST_LEAVE_GROUP),
			greq6, jsonl_group_req6
		}
	};

	for (unsigned int i = 0; i < ARRAY_SIZE(opts); ++i) {
		const int size = sizeof(*opts[i].val);

		/* optlen < 0, EINVAL */
		test_opt(opts[i].level, opts[i].str_level,
			 opts[i].name, opts[i].str_name,
			 opts[i].val, -1, NULL);

		/* optlen < sizeof(struct group_req), EINVAL */
		test_opt(opts[i].level, opts[i].str_level,
			 opts[i].name, opts[i].str_name,
			 opts[i].val, size - 1, NULL);

		/* optval EFAULT */
		test_opt(opts[i].level, opts[i].str_level,
			 opts[i].name, opts[i].str_name,
			 (const char *) opts[i].val + 1, size, NULL);

		/* classic */
		test_opt(opts[i].level, opts[i].str_level,
			 opts[i].name, opts[i].str_name,
			 opts[i].val, size, opts[i].print_val);

		/* optlen > sizeof(struct group_req), shortened */
		test_opt(opts[i].level, opts[i].str_level,
			 opts[i].name, opts[i].str_name,
			 opts[i].val, INT_MAX, opts[i].print_val);
	}

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_setsockopt"
		    " && MCAST_JOIN_GROUP && MCAST_LEAVE_GROUP")

#endif
