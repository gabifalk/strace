/*
 * Check JSONL output of socket addresses of the connect syscall:
 * AX.25 addresses, a negated AF_TIPC scope, the bytes following the
 * family of an AF_RXRPC transport address, the hardware address of
 * an AF_PACKET address, and the scope_id of a link-local AF_INET6
 * address.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_connect

# include "jsonl.h"

# include <errno.h>
# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/socket.h>
# include <arpa/inet.h>
# include <netinet/in.h>
# include <linux/ax25.h>
# include <linux/if_arp.h>
# include <linux/if_ether.h>
# include <linux/if_packet.h>
# include <linux/tipc.h>

struct rxrpc_addr {
	uint16_t srx_family;
	uint16_t srx_service;
	uint16_t transport_type;
	uint16_t transport_len;
	uint16_t transport_family;
	unsigned char transport_data[26];
};

static bool merged;

static void
jsonl_family(int family, const char *sym)
{
	printf("\"type\": \"sockaddr\", \"family\": {");
	jsonl_const(family, true, sym, NULL);
	printf("}, \"fields\": {");
}

static void
test_connect(const void *addr, int len, void (*print_addr)(const void *))
{
	const char *const side = merged ? "entry" : NULL;
	long rc = syscall(__NR_connect, -1, addr, len);
	int saved_errno = errno;

	if (merged)
		jsonl_syscall_merged_open("connect", __NR_connect);
	else
		jsonl_syscall_open("connect", __NR_connect, true);
	jsonl_arg_open(0, "sockfd", side);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "addr", side);
	print_addr(addr);
	printf("}");
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "addrlen", side);
	jsonl_decimal(len);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("connect", __NR_connect, false);
	}
	printf("], ");
	errno = saved_errno;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

static void
print_ax25(const void *addr)
{
	const struct sockaddr_ax25 *const sa = addr;

	jsonl_family(ARG_STR(AF_AX25));
	printf("\"sax25_call\": {\"type\": \"ax25_address\", \"value\": ");
	jsonl_bytes_n(sa->sax25_call.ax25_call,
		      sizeof(sa->sax25_call.ax25_call));
	printf("}");
	jsonl_field_decimal("sax25_ndigis", false, sa->sax25_ndigis);
}

static void
print_tipc(const void *addr)
{
	const struct sockaddr_tipc *const sa = addr;

	jsonl_family(ARG_STR(AF_TIPC));
	jsonl_field_const("addrtype", true, ARG_STR(TIPC_SOCKET_ADDR), NULL);
	printf(", \"scope\": {\"type\": \"negated\", \"raw\": \"%d\","
	       " \"value\": {", (signed char) sa->scope);
	jsonl_const(-(signed char) sa->scope, true, "TIPC_NODE_SCOPE", NULL);
	printf("}}, \"addr\": {");
	jsonl_struct_open();
	printf("\"id\": {");
	jsonl_struct_open();
	jsonl_field_unsigned("ref", true, sa->addr.id.ref);
	jsonl_field_hex("node", false, sa->addr.id.node);
	jsonl_struct_close();
	printf("}");
	jsonl_struct_close();
	printf("}");
}

static void
print_rxrpc(const void *addr)
{
	const struct rxrpc_addr *const sa = addr;

	jsonl_family(ARG_STR(AF_RXRPC));
	jsonl_field_const("srx_service", true, 1, "CM_SERVICE", NULL);
	jsonl_field_const("transport_type", false,
			  ARG_STR(SOCK_DGRAM), NULL);
	jsonl_field_unsigned("transport_len", false, sa->transport_len);
	printf(", \"transport\": {");
	jsonl_struct_open();
	jsonl_field_const("family", true, ARG_STR(AF_UNIX), NULL);
	printf(", \"addr_data\": {\"type\": \"string\", \"value\": ");
	jsonl_bytes_n((const char *) sa->transport_data,
		      sa->transport_len - sizeof(sa->transport_family));
	printf("}");
	jsonl_struct_close();
	printf("}");
}

static void
print_packet(const void *addr)
{
	const struct sockaddr_ll *const sa = addr;

	jsonl_family(ARG_STR(AF_PACKET));
	printf("\"sll_protocol\": {");
	jsonl_const(ETH_P_IP, true, "ETH_P_IP", NULL);
	jsonl_byteorder_big(2);
	printf("}");
	jsonl_field_ifindex("sll_ifindex", false, sa->sll_ifindex, NULL);
	jsonl_field_const("sll_hatype", false, ARG_STR(ARPHRD_ETHER), NULL);
	jsonl_field_const("sll_pkttype", false, ARG_STR(PACKET_HOST), NULL);
	jsonl_field_unsigned("sll_halen", false, sa->sll_halen);
	printf(", \"sll_addr\": {");
	jsonl_array_open();
	for (unsigned int i = 0; i < sa->sll_halen; ++i) {
		printf("%s{", i ? ", " : "");
		jsonl_hex(sa->sll_addr[i]);
		printf("}");
	}
	jsonl_array_close();
	printf("}");
}

static unsigned int lo_ifindex;

static void
print_in6(const void *addr)
{
	const struct sockaddr_in6 *const sa = addr;

	jsonl_family(ARG_STR(AF_INET6));
	printf("\"addr\": \"fe80::1\", \"port\": {");
	jsonl_unsigned(ntohs(sa->sin6_port));
	jsonl_byteorder_big(2);
	printf("}, \"flowinfo\": {");
	jsonl_unsigned(ntohl(sa->sin6_flowinfo));
	jsonl_byteorder_big(4);
	printf("}, \"scope_id\": {");
	jsonl_ifindex(lo_ifindex, "lo");
	printf("}");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	lo_ifindex = ifindex_lo();
	if (!lo_ifindex)
		perror_msg_and_skip("lo");

	TAIL_ALLOC_OBJECT_CONST_PTR(struct sockaddr_ax25, ax25);
	static const char call[] = "TEST  ";

	ax25->sax25_family = AF_AX25;
	for (unsigned int i = 0; i < 6; ++i)
		ax25->sax25_call.ax25_call[i] = call[i] << 1;
	ax25->sax25_call.ax25_call[6] = 7 << 1;
	ax25->sax25_ndigis = 0;
	test_connect(ax25, sizeof(*ax25), print_ax25);

	TAIL_ALLOC_OBJECT_CONST_PTR(struct sockaddr_tipc, tipc);
	memset(tipc, 0, sizeof(*tipc));
	tipc->family = AF_TIPC;
	tipc->addrtype = TIPC_SOCKET_ADDR;
	tipc->scope = -TIPC_NODE_SCOPE;
	tipc->addr.id.ref = 42;
	tipc->addr.id.node = 0xfeedbabe;
	test_connect(tipc, sizeof(*tipc), print_tipc);

	TAIL_ALLOC_OBJECT_CONST_PTR(struct rxrpc_addr, rxrpc);
	memset(rxrpc, 0, sizeof(*rxrpc));
	rxrpc->srx_family = AF_RXRPC;
	rxrpc->srx_service = 1;
	rxrpc->transport_type = SOCK_DGRAM;
	rxrpc->transport_len = 6;
	rxrpc->transport_family = AF_UNIX;
	memcpy(rxrpc->transport_data, "data", 4);
	test_connect(rxrpc, sizeof(*rxrpc), print_rxrpc);

	TAIL_ALLOC_OBJECT_CONST_PTR(struct sockaddr_ll, ll);
	memset(ll, 0, sizeof(*ll));
	ll->sll_family = AF_PACKET;
	ll->sll_protocol = htons(ETH_P_IP);
	ll->sll_ifindex = 0xdeadbeef;
	ll->sll_hatype = ARPHRD_ETHER;
	ll->sll_pkttype = PACKET_HOST;
	ll->sll_halen = 6;
	memcpy(ll->sll_addr, "\x00\x11\x22\x33\x44\x55", 6);
	test_connect(ll, sizeof(*ll), print_packet);

	TAIL_ALLOC_OBJECT_CONST_PTR(struct sockaddr_in6, in6);
	memset(in6, 0, sizeof(*in6));
	in6->sin6_family = AF_INET6;
	in6->sin6_port = htons(8080);
	in6->sin6_flowinfo = htonl(42);
	inet_pton(AF_INET6, "fe80::1", &in6->sin6_addr);
	in6->sin6_scope_id = lo_ifindex;
	test_connect(in6, sizeof(*in6), print_in6);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_connect")

#endif
