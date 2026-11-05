/*
 * Check JSONL output of the getsockname syscall.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"
#include "scno.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define TEST_SOCKET "jsonl-getsockname.socket"

static bool merged;

static void
jsonl_open(int fd)
{
	const char *side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("getsockname", __NR_getsockname);
	else
		jsonl_syscall_open("getsockname", __NR_getsockname, true);
	jsonl_arg_open(0, "sockfd", side);
	printf("\"type\": \"fd\", \"raw\": \"%d\"", fd);
	jsonl_arg_close(side);
}

static void
jsonl_addr(const void *p)
{
	if (p)
		printf("\"type\": \"addr\", \"raw\": \"%#lx\"",
		       (unsigned long) (uintptr_t) p);
	else
		printf("\"type\": \"addr\", \"raw\": null");
}

static void
jsonl_addrlen(socklen_t len, const char *extra)
{
	printf("\"type\": \"decimal\", \"raw\": \"%u\", \"indirect\": true%s",
	       (unsigned int) len, extra);
}

/* Both addr and addrlen are printed as pointers on entering. */
static void
jsonl_undecoded(int fd, const void *addr, const void *plen, long rc)
{
	const char *side = merged ? "entry" : NULL;

	jsonl_open(fd);
	printf(", ");
	jsonl_arg_open(1, "addr", side);
	jsonl_addr(addr);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "addrlen", side);
	jsonl_addr(plen);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("getsockname", __NR_getsockname, false);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
}

/*
 * addrlen is read on entering, addr and addrlen are decoded on exiting.
 * sun_path_len < 0 means the path is not printed, and path_addr means
 * the address is printed as a pointer.
 */
static void
jsonl_decoded(int fd, const struct sockaddr_un *addr, const void *path_addr,
	      int sun_path_len, socklen_t ulen, socklen_t rlen, long rc)
{
	const char *changed = ulen != rlen ? ", \"changed\": true" : "";

	jsonl_open(fd);
	printf(", ");
	if (!merged) {
		jsonl_arg_open(2, "addrlen", NULL);
		jsonl_addrlen(ulen, ", \"inout\": true");
		jsonl_arg_close(NULL);
		printf("]}\n");
		jsonl_syscall_open("getsockname", __NR_getsockname, false);
	}
	jsonl_arg_open(1, "addr", merged ? "exit" : NULL);
	if (path_addr) {
		jsonl_addr(path_addr);
	} else {
		printf("\"type\": \"sockaddr\","
		       " \"family\": {\"type\": \"const\", \"raw\": \"0x1\","
		       " \"sym\": \"AF_UNIX\"}");
		if (sun_path_len >= 0) {
			printf(", \"path\": {\"type\": \"sun_path\", \"value\": ");
			jsonl_bytes_n(addr->sun_path, sun_path_len);
			printf("}");
		}
	}
	jsonl_arg_close(merged ? "exit" : NULL);
	printf(", ");
	if (merged) {
		jsonl_arg_open(2, "addrlen", "entry");
		jsonl_addrlen(ulen, ", \"inout\": true");
		printf("}, \"exit\": {");
		jsonl_addrlen(rlen, changed);
		jsonl_arg_close("exit");
	} else {
		jsonl_arg_open(2, "addrlen", NULL);
		jsonl_addrlen(rlen, changed);
		jsonl_arg_close(NULL);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	int lfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (lfd < 0)
		perror_msg_and_skip("socket");

	(void) unlink(TEST_SOCKET);

	const struct sockaddr_un un = {
		.sun_family = AF_UNIX,
		.sun_path = TEST_SOCKET
	};

	if (bind(lfd, (const void *) &un, sizeof(un)))
		perror_msg_and_skip("bind");

	TAIL_ALLOC_OBJECT_CONST_PTR(socklen_t, plen);
	*plen = sizeof(struct sockaddr_un);
	struct sockaddr_un *addr = tail_alloc(*plen);
	socklen_t ulen;
	long rc;

	ulen = *plen;
	rc = getsockname(lfd, (void *) addr, plen);
	if (rc < 0)
		perror_msg_and_skip("getsockname");
	jsonl_decoded(lfd, addr, NULL, strlen(addr->sun_path),
		      ulen, *plen, rc);

	memset(addr, 0, sizeof(*addr));
	ulen = *plen;
	rc = getsockname(lfd, (void *) addr, plen);
	if (rc < 0)
		perror_msg_and_skip("getsockname");
	jsonl_decoded(lfd, addr, NULL, strlen(addr->sun_path),
		      ulen, *plen, rc);

	rc = getsockname(lfd, (void *) addr, 0);
	jsonl_undecoded(lfd, addr, NULL, rc);

	rc = getsockname(lfd, 0, 0);
	jsonl_undecoded(lfd, NULL, NULL, rc);

	rc = getsockname(lfd, (void *) addr, plen + 1);
	jsonl_undecoded(lfd, addr, plen + 1, rc);

	const size_t offsetof_sun_path = offsetof(struct sockaddr_un, sun_path);
	*plen = offsetof_sun_path;
	memset(addr->sun_path, 'A', sizeof(addr->sun_path));

	ulen = *plen;
	rc = getsockname(lfd, (void *) addr, plen);
	if (rc < 0)
		perror_msg_and_skip("getsockname");
	jsonl_decoded(lfd, addr, NULL, -1, ulen, *plen, rc);

	++addr;
	*plen = sizeof(struct sockaddr);
	addr = (void *) addr - *plen;

	ulen = *plen;
	rc = getsockname(lfd, (void *) addr, plen);
	if (rc < 0)
		perror_msg_and_skip("getsockname");
	jsonl_decoded(lfd, addr, NULL,
		      (int) (sizeof(struct sockaddr) - offsetof_sun_path),
		      ulen, *plen, rc);

	ulen = *plen;
	rc = getsockname(lfd, (void *) addr, plen);
	jsonl_decoded(lfd, addr, addr, -1, ulen, *plen, rc);

	(void) unlink(TEST_SOCKET);

	jsonl_exit_event(0);
	return 0;
}
