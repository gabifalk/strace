/*
 * Check JSONL output of ppoll: poll_result return values with the
 * descriptors with returned events and the time left, or a timeout.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_ppoll

# include "jsonl.h"
# include "kernel_old_timespec.h"

# include <poll.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

static bool merged;
static const char *side;

static void
print_pollfd(int fd, const char *name, unsigned int bits, const char *sym,
	     bool dflt_null)
{
	printf("{");
	jsonl_struct_open();
	jsonl_field_open("fd", true);
	jsonl_fd(fd, NULL, false);
	jsonl_field_close();
	jsonl_field_open(name, false);
	if (dflt_null)
		jsonl_flags_open_dflt_null(bits);
	else
		jsonl_flags_open(bits);
	jsonl_flags_const(bits, sym, true);
	jsonl_flags_close();
	jsonl_field_close();
	jsonl_struct_close();
	printf("}");
}

static void
open_event(const int *fds, unsigned int nfds, const kernel_old_timespec_t *ts)
{
	if (merged)
		jsonl_syscall_merged_open("ppoll", __NR_ppoll);
	else
		jsonl_syscall_open("ppoll", __NR_ppoll, true);
	jsonl_arg_open(0, "fds", side);
	jsonl_array_open();
	if (nfds) {
		print_pollfd(fds[0], "events", POLLIN, "POLLIN", false);
		printf(", ");
		print_pollfd(fds[1], "events", POLLOUT, "POLLOUT", false);
	}
	jsonl_array_close();
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "nfds", side);
	jsonl_unsigned(nfds);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "tmo_p", side);
	jsonl_timespec(ts->tv_sec, ts->tv_nsec);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(3, "sigmask", side);
	jsonl_addr(NULL);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(4, "sigsetsize", side);
	jsonl_unsigned(8);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("ppoll", __NR_ppoll, false);
	}
	printf("], \"return\": {\"type\": \"poll_result\"");
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

	struct pollfd pfd[2] = {
		{ .fd = fds[0], .events = POLLIN },
		{ .fd = fds[1], .events = POLLOUT },
	};
	kernel_old_timespec_t ts = { .tv_sec = 1 };
	const kernel_old_timespec_t ts_in = ts;
	long rc = syscall(__NR_ppoll, pfd, 2, &ts, NULL, 8);
	open_event(fds, 2, &ts_in);
	printf(", \"raw\": \"%ld\", \"fds\": {", rc);
	jsonl_array_open();
	print_pollfd(fds[0], "revents", POLLIN, "POLLIN", true);
	printf(", ");
	print_pollfd(fds[1], "revents", POLLOUT, "POLLOUT", true);
	jsonl_array_close();
	printf("}, \"left\": {");
	jsonl_timespec(ts.tv_sec, ts.tv_nsec);
	printf("}}}\n");

	const kernel_old_timespec_t ts0 = { 0 };
	ts = ts0;
	rc = syscall(__NR_ppoll, pfd, 0, &ts, NULL, 8);
	open_event(fds, 0, &ts0);
	printf(", \"raw\": \"%ld\"}}\n", rc);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ppoll")

#endif
