/*
 * Check JSONL output of struct packet_mreq socket options.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_setsockopt

# include "jsonl.h"

# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/socket.h>
# include <linux/if_packet.h>

static bool merged;

static void
jsonl_packet_mreq(const struct packet_mreq *pmreq, const char *type_str)
{
	unsigned int alen = pmreq->mr_alen;

	if (alen > ARRAY_SIZE(pmreq->mr_address))
		alen = ARRAY_SIZE(pmreq->mr_address);

	jsonl_struct_open();
	jsonl_field_open("mr_ifindex", true);
	jsonl_ifindex(pmreq->mr_ifindex, NULL);
	jsonl_field_close();
	jsonl_field_open("mr_type", false);
	jsonl_const(pmreq->mr_type, true, type_str, "PACKET_MR_???");
	jsonl_field_close();
	jsonl_field_open("mr_alen", false);
	jsonl_unsigned(pmreq->mr_alen);
	jsonl_field_close();
	jsonl_field_open("mr_address", false);
	jsonl_mac(pmreq->mr_address, alen);
	jsonl_field_close();
	jsonl_struct_close();
}

/*
 * The option value is printed as a pointer when decoded is false,
 * type_str is NULL when mr_type is not a known constant.
 */
static void
test_opt(unsigned int optname, const char *optname_str,
	 const struct packet_mreq *pmreq, socklen_t len,
	 bool decoded, const char *type_str)
{
	const char *side = merged ? "entry" : NULL;
	long rc = syscall(__NR_setsockopt, -1, SOL_PACKET, optname, pmreq, len);
	int saved_errno = errno;

	if (merged)
		jsonl_syscall_merged_open("setsockopt", __NR_setsockopt);
	else
		jsonl_syscall_open("setsockopt", __NR_setsockopt, true);

	jsonl_arg_open(0, "sockfd", side);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(1, "level", side);
	jsonl_const(SOL_PACKET, true, "SOL_PACKET", NULL);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(2, "optname", side);
	jsonl_const(optname, true, optname_str, "PACKET_???");
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(3, "optval", side);
	if (decoded)
		jsonl_packet_mreq(pmreq, type_str);
	else
		jsonl_addr(pmreq);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(4, "optlen", side);
	jsonl_decimal((int) len);
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

static void
test_packet_mreq(const int optname, const char *const optname_str)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(struct packet_mreq, pmreq);
	socklen_t len = sizeof(struct packet_mreq);

	/* setsockopt with optname unknown */
	test_opt(-1, NULL, NULL, 0, false, NULL);

	/* setsockopt with mr_type unknown */
	pmreq->mr_ifindex = 0;
	pmreq->mr_type = 0xffff;
	pmreq->mr_alen = ARRAY_SIZE(pmreq->mr_address);
	for (unsigned int i = 0; i < ARRAY_SIZE(pmreq->mr_address); ++i)
		pmreq->mr_address[i] = i + 1;
	test_opt(optname, optname_str, pmreq, len, true, NULL);

	/* setsockopt with mr_type unknown and mr_alen > sizeof(mr_address) */
	pmreq->mr_alen = ARRAY_SIZE(pmreq->mr_address) + 1;
	test_opt(optname, optname_str, pmreq, len, true, NULL);

	/* setsockopt with mr_type unknown and mr_alen < sizeof(mr_address) */
	pmreq->mr_alen = ARRAY_SIZE(pmreq->mr_address) - 1;
	test_opt(optname, optname_str, pmreq, len, true, NULL);

	/* setsockopt with valid mr_type */
	pmreq->mr_alen = ARRAY_SIZE(pmreq->mr_address);
	static const struct {
		unsigned short type;
		const char *const type_str;
	} a[] = {
		{ ARG_STR(PACKET_MR_MULTICAST) },
		{ ARG_STR(PACKET_MR_PROMISC) },
		{ ARG_STR(PACKET_MR_ALLMULTI) },
# ifdef PACKET_MR_UNICAST
		{ ARG_STR(PACKET_MR_UNICAST) },
# endif
	};

	for (unsigned int i = 0; i < ARRAY_SIZE(a); i++) {
		pmreq->mr_type = a[i].type;
		test_opt(optname, optname_str, pmreq, len, true, a[i].type_str);
	}

	/* setsockopt with optlen larger than usual */
	test_opt(optname, optname_str, pmreq, len + 1, false, NULL);
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	test_packet_mreq(ARG_STR(PACKET_ADD_MEMBERSHIP));
	test_packet_mreq(ARG_STR(PACKET_DROP_MEMBERSHIP));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_setsockopt")

#endif
