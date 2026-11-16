/*
 * Check JSONL output of recvfrom: the addrlen argument marked inout on
 * entering and changed on exiting when the kernel has rewritten it.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_recvfrom

# include "jsonl.h"

# include <errno.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/socket.h>
# include <sys/un.h>

# define RECV_SOCKET "jsonl-recvfrom.recv.socket"
# define SEND_SOCKET "jsonl-recvfrom.send.socket"

static bool merged;

static void
print_flags(void)
{
	jsonl_flags_open(MSG_DONTWAIT);
	jsonl_flags_const(MSG_DONTWAIT, "MSG_DONTWAIT", true);
	jsonl_flags_close();
}

static void
test_recvfrom(int fd, char *buf, struct sockaddr_un *addr, socklen_t *plen)
{
	const socklen_t ulen = *plen;
	long rc = syscall(__NR_recvfrom, fd, buf, 1, MSG_DONTWAIT, addr, plen);
	int saved_errno = errno;
	const char *const side = merged ? "exit" : NULL;

	if (merged)
		jsonl_syscall_merged_open("recvfrom", __NR_recvfrom);
	else
		jsonl_syscall_open("recvfrom", __NR_recvfrom, true);
	jsonl_arg_open(0, "sockfd", merged ? "entry" : NULL);
	jsonl_fd(fd, NULL, false);
	jsonl_arg_close(merged ? "entry" : NULL);
	printf(", ");
	if (!merged) {
		jsonl_arg_open(5, "addrlen", NULL);
		jsonl_addrlen(ulen, ", \"inout\": true");
		jsonl_arg_close(NULL);
		printf("]}\n");
		jsonl_syscall_open("recvfrom", __NR_recvfrom, false);
	}

	jsonl_arg_open(1, "buf", side);
	if (rc < 0) {
		jsonl_addr(buf);
	} else {
		printf("\"type\": \"string\", \"value\": ");
		jsonl_bytes_n(buf, rc);
	}
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "len", side);
	jsonl_unsigned(1);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(3, "flags", side);
	print_flags();
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(4, "src_addr", side);
	if (rc < 0) {
		jsonl_addr(addr);
	} else {
		printf("\"type\": \"sockaddr\", \"family\": {");
		jsonl_const(AF_UNIX, true, "AF_UNIX", NULL);
		printf("}, \"fields\": {\"path\": "
		       "{\"type\": \"sun_path\", \"value\": ");
		jsonl_bytes(addr->sun_path);
		printf("}}");
	}
	jsonl_arg_close(side);
	printf(", ");
	const char *const changed =
		rc >= 0 && ulen != *plen ? ", \"changed\": true" : "";
	if (merged) {
		jsonl_arg_open(5, "addrlen", "entry");
		jsonl_addrlen(ulen, ", \"inout\": true");
		printf("}, \"exit\": {");
		jsonl_addrlen(rc >= 0 ? *plen : ulen, changed);
		jsonl_arg_close("exit");
	} else {
		jsonl_arg_open(5, "addrlen", NULL);
		jsonl_addrlen(rc >= 0 ? *plen : ulen, changed);
		jsonl_arg_close(NULL);
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

	struct sockaddr_un recv_addr = { .sun_family = AF_UNIX,
					 .sun_path = RECV_SOCKET };
	struct sockaddr_un send_addr = { .sun_family = AF_UNIX,
					 .sun_path = SEND_SOCKET };
	int recv_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	int send_fd = socket(AF_UNIX, SOCK_DGRAM, 0);

	if (recv_fd < 0 || send_fd < 0)
		perror_msg_and_skip("socket");
	(void) unlink(RECV_SOCKET);
	(void) unlink(SEND_SOCKET);
	if (bind(recv_fd, (void *) &recv_addr, sizeof(recv_addr)) ||
	    bind(send_fd, (void *) &send_addr, sizeof(send_addr)))
		perror_msg_and_skip("bind");
	if (connect(send_fd, (void *) &recv_addr, sizeof(recv_addr)))
		perror_msg_and_skip("connect");

	TAIL_ALLOC_OBJECT_CONST_PTR(char, buf);
	TAIL_ALLOC_OBJECT_CONST_PTR(struct sockaddr_un, addr);
	TAIL_ALLOC_OBJECT_CONST_PTR(socklen_t, plen);

	*plen = sizeof(*addr);
	test_recvfrom(recv_fd, buf, addr, plen);

	if (send(send_fd, "A", 1, 0) != 1)
		perror_msg_and_skip("send");
	memset(addr, 0, sizeof(*addr));
	*plen = sizeof(*addr);
	test_recvfrom(recv_fd, buf, addr, plen);

	if (send(send_fd, "B", 1, 0) != 1)
		perror_msg_and_skip("send");
	memset(addr, 0, sizeof(*addr));
	test_recvfrom(recv_fd, buf, addr, plen);

	(void) unlink(RECV_SOCKET);
	(void) unlink(SEND_SOCKET);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_recvfrom")

#endif
