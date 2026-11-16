/*
 * Check JSONL output of recvmsg: struct msghdr delivered as a split
 * struct whose msg_namelen is marked changed, with no exit half after
 * a failed syscall, and control messages.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_recvmsg

# include "jsonl.h"

# include <errno.h>
# include <stddef.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/socket.h>
# include <sys/un.h>

# define RECV_SOCKET "jsonl-recvmsg.recv.socket"
# define SEND_SOCKET "jsonl-recvmsg.send.socket"

static bool merged;

static void
print_entering_msghdr(int namelen)
{
	jsonl_struct_open();
	jsonl_field_decimal("msg_namelen", true, namelen);
	jsonl_struct_close();
	printf(", \"split\": true");
}

static void
print_flags(void)
{
	jsonl_flags_open(MSG_DONTWAIT);
	jsonl_flags_const(MSG_DONTWAIT, "MSG_DONTWAIT", true);
	jsonl_flags_close();
}

static void
print_msghdr(const struct msghdr *msg, int namelen, long rc, int fd)
{
	const struct sockaddr_un *const un = msg->msg_name;

	jsonl_struct_open();
	printf("\"msg_name\": {\"type\": \"sockaddr\", \"family\": ");
	printf("{\"type\": \"const\", \"raw\": \"0x1\", \"sym\": \"AF_UNIX\"}");
	printf(", \"fields\": {\"path\": {\"type\": \"sun_path\", \"value\": ");
	jsonl_bytes(un->sun_path);
	printf("}}}");
	jsonl_field_open("msg_namelen", false);
	jsonl_decimal(msg->msg_namelen);
	if ((int) msg->msg_namelen != namelen)
		printf(", \"changed\": true");
	jsonl_field_close();
	printf(", \"msg_iov\": {");
	jsonl_array_open();
	printf("{");
	jsonl_struct_open();
	printf("\"iov_base\": {\"type\": \"string\", \"value\": ");
	jsonl_bytes_n(msg->msg_iov[0].iov_base, rc);
	printf("}");
	jsonl_field_unsigned("iov_len", false, msg->msg_iov[0].iov_len);
	jsonl_struct_close();
	printf("}");
	jsonl_array_close();
	printf("}");
	jsonl_field_unsigned("msg_iovlen", false, msg->msg_iovlen);
	if (msg->msg_controllen) {
		const struct cmsghdr *const cmsg = CMSG_FIRSTHDR(msg);

		printf(", \"msg_control\": {");
		jsonl_array_open();
		printf("{");
		jsonl_struct_open();
		jsonl_field_unsigned("cmsg_len", true, cmsg->cmsg_len);
		jsonl_field_open("cmsg_level", false);
		jsonl_const(SOL_SOCKET, true, "SOL_SOCKET", NULL);
		jsonl_field_close();
		jsonl_field_open("cmsg_type", false);
		jsonl_const(SCM_RIGHTS, true, "SCM_RIGHTS", NULL);
		jsonl_field_close();
		printf(", \"cmsg_data\": {");
		jsonl_array_open();
		printf("{");
		jsonl_fd(fd, NULL, false);
		printf("}");
		jsonl_array_close();
		printf("}");
		jsonl_struct_close();
		printf("}");
		jsonl_array_close();
		printf("}");
	}
	jsonl_field_unsigned("msg_controllen", false, msg->msg_controllen);
	jsonl_field_open("msg_flags", false);
	jsonl_flags_open(0);
	jsonl_flags_close();
	jsonl_field_close();
	jsonl_struct_close();
	printf(", \"split\": true");
}

static void
test_recvmsg(int fd, struct msghdr *msg)
{
	const int namelen = msg->msg_namelen;
	long rc = syscall(__NR_recvmsg, fd, msg, MSG_DONTWAIT);
	int saved_errno = errno;
	int rfd = -1;

	if (rc >= 0 && msg->msg_controllen)
		memcpy(&rfd, CMSG_DATA(CMSG_FIRSTHDR(msg)), sizeof(rfd));

	if (merged)
		jsonl_syscall_merged_open("recvmsg", __NR_recvmsg);
	else
		jsonl_syscall_open("recvmsg", __NR_recvmsg, true);
	jsonl_arg_open(0, "sockfd", merged ? "entry" : NULL);
	jsonl_fd(fd, NULL, false);
	jsonl_arg_close(merged ? "entry" : NULL);
	printf(", ");
	jsonl_arg_open(1, "msg", merged ? "entry" : NULL);
	print_entering_msghdr(namelen);
	if (merged) {
		printf("}");
		if (rc >= 0) {
			printf(", \"exit\": {");
			print_msghdr(msg, namelen, rc, rfd);
			printf("}");
		}
		printf("}, ");
	} else {
		printf("}]}\n");
		jsonl_syscall_open("recvmsg", __NR_recvmsg, false);
		if (rc >= 0) {
			jsonl_arg_open(1, "msg", NULL);
			print_msghdr(msg, namelen, rc, rfd);
			printf("}, ");
		}
	}
	jsonl_arg_open(2, "flags", merged ? "exit" : NULL);
	print_flags();
	jsonl_arg_close(merged ? "exit" : NULL);
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

	struct sockaddr_un name;
	char buf;
	struct iovec iov = { .iov_base = &buf, .iov_len = 1 };
	struct msghdr msg = {
		.msg_name = &name,
		.msg_namelen = sizeof(name),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};

	test_recvmsg(recv_fd, &msg);

	if (send(send_fd, "A", 1, 0) != 1)
		perror_msg_and_skip("send");
	memset(&name, 0, sizeof(name));
	msg.msg_namelen = sizeof(name);
	test_recvmsg(recv_fd, &msg);

	union {
		struct cmsghdr align;
		char buf[CMSG_SPACE(sizeof(int))];
	} control;
	struct msghdr smsg = {
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = &control,
		.msg_controllen = sizeof(control.buf),
	};
	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&smsg);

	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	memcpy(CMSG_DATA(cmsg), &send_fd, sizeof(send_fd));
	buf = 'B';
	if (sendmsg(send_fd, &smsg, 0) != 1)
		perror_msg_and_skip("sendmsg");

	memset(&name, 0, sizeof(name));
	memset(&control, 0, sizeof(control));
	msg.msg_namelen = sizeof(name);
	msg.msg_control = &control;
	msg.msg_controllen = sizeof(control.buf);
	test_recvmsg(recv_fd, &msg);

	(void) unlink(RECV_SOCKET);
	(void) unlink(SEND_SOCKET);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_recvmsg")

#endif
