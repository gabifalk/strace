/*
 * Check JSONL output of select: fd_set arguments and select_result return
 * values with ready descriptor sets and the time left, or a timeout.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_select && !defined __NR__newselect

# include "jsonl.h"
# include "kernel_timeval.h"

# include <stdio.h>
# include <string.h>
# include <sys/select.h>
# include <unistd.h>

static bool merged;
static const char *side;

static void
print_fd_set(const int *fds, unsigned int n)
{
	printf("\"type\": \"fd_set\", \"elems\": [");
	for (unsigned int i = 0; i < n; ++i) {
		printf("%s{", i ? ", " : "");
		jsonl_fd(fds[i], NULL, false);
		printf("}");
	}
	printf("]");
}

static void
print_arg_fd_set(unsigned int index, const char *name,
		 const int *fds, unsigned int n)
{
	printf(", ");
	jsonl_arg_open(index, name, side);
	if (fds)
		print_fd_set(fds, n);
	else
		jsonl_addr(NULL);
	jsonl_arg_close(side);
}

static void
open_event(int nfds, const int *rfds, unsigned int nr,
	   const int *wfds, unsigned int nw, const kernel_old_timeval_t *tv)
{
	if (merged)
		jsonl_syscall_merged_open("select", __NR_select);
	else
		jsonl_syscall_open("select", __NR_select, true);
	jsonl_arg_open(0, "nfds", side);
	jsonl_decimal(nfds);
	jsonl_arg_close(side);
	print_arg_fd_set(1, "readfds", rfds, nr);
	print_arg_fd_set(2, "writefds", wfds, nw);
	print_arg_fd_set(3, "exceptfds", NULL, 0);
	printf(", ");
	jsonl_arg_open(4, "timeout", side);
	jsonl_timeval(tv->tv_sec, tv->tv_usec);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("select", __NR_select, false);
	}
	printf("], \"return\": {\"type\": \"select_result\"");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	int fds[2];
	if (pipe(fds))
		perror_msg_and_fail("pipe");
	if (write(fds[1], "x", 1) != 1)
		perror_msg_and_fail("write");

	fd_set r, w;
	FD_ZERO(&r);
	FD_ZERO(&w);
	FD_SET(fds[0], &r);
	FD_SET(fds[1], &w);
	kernel_old_timeval_t tv = { .tv_sec = 1 };
	const kernel_old_timeval_t tv_in = tv;
	long rc = syscall(__NR_select, fds[1] + 1, &r, &w, NULL, &tv);
	open_event(fds[1] + 1, &fds[0], 1, &fds[1], 1, &tv_in);
	printf(", \"raw\": \"%ld\", \"in\": {", rc);
	print_fd_set(&fds[0], 1);
	printf("}, \"out\": {");
	print_fd_set(&fds[1], 1);
	printf("}, \"left\": {");
	jsonl_timeval(tv.tv_sec, tv.tv_usec);
	printf("}}}\n");

	FD_ZERO(&r);
	const kernel_old_timeval_t tv0 = { 0 };
	tv = tv0;
	rc = syscall(__NR_select, fds[1] + 1, &r, NULL, NULL, &tv);
	open_event(fds[1] + 1, fds, 0, NULL, 0, &tv0);
	printf(", \"raw\": \"%ld\"}}\n", rc);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_select && !__NR__newselect")

#endif
