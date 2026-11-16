/*
 * Check JSONL output of the signal mask of a signalfd descriptor (-yy):
 * the sigmask of the signalfd fd_info, a signal set.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined HAVE_SYS_SIGNALFD_H && defined HAVE_SIGNALFD

# include "jsonl.h"

# include <signal.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/signalfd.h>

static void
print_sigset(const bool chld)
{
	printf("\"type\": \"bitset\", \"raw\": \"%#lx\", \"elems\": [{",
	       (1UL << (SIGUSR2 - 1)) | (chld ? 1UL << (SIGCHLD - 1) : 0));
	jsonl_signal(SIGUSR2, "SIGUSR2");
	if (chld) {
		printf("}, {");
		jsonl_signal(SIGCHLD, "SIGCHLD");
	}
	printf("}]");
}

static void
print_signalfd(const int fd, const bool chld)
{
	printf("\"type\": \"fd\", \"raw\": \"%d\", \"fd_info\":"
	       " {\"type\": \"signalfd\", \"sigmask\": {", fd);
	print_sigset(chld);
	printf("}}");
}

static bool merged;

static int
test_signalfd4(const int fd, const bool chld)
{
	const char *const side = merged ? "entry" : NULL;
	sigset_t mask;

	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR2);
	if (chld)
		sigaddset(&mask, SIGCHLD);

	const int rc = signalfd(fd, &mask, 0);
	if (rc < 0)
		perror_msg_and_skip("signalfd");

	if (merged)
		jsonl_syscall_merged_open("signalfd4", __NR_signalfd4);
	else
		jsonl_syscall_open("signalfd4", __NR_signalfd4, true);
	jsonl_arg_open(0, "fd", side);
	if (fd < 0)
		jsonl_fd(fd, NULL, false);
	else
		print_signalfd(fd, !chld);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "mask", side);
	print_sigset(chld);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "sizemask", side);
	jsonl_unsigned(get_sigset_size());
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(3, "flags", side);
	jsonl_flags_open(0);
	jsonl_flags_close();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("signalfd4", __NR_signalfd4, false);
	}
	printf("], \"return\": {");
	print_signalfd(rc, chld);
	printf("}}\n");

	return rc;
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	test_signalfd4(test_signalfd4(-1, true), false);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("HAVE_SYS_SIGNALFD_H && HAVE_SIGNALFD")

#endif
