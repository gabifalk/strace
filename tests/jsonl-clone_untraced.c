/*
 * Check JSONL output of clone whose CLONE_UNTRACED flag strace removes
 * under --seccomp-bpf: the flags argument carries "sanitized": true.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_clone \
 && (defined __x86_64__ || defined __i386__ || defined __aarch64__)

# include "jsonl.h"

# include <signal.h>
# include <stdio.h>
# include <string.h>
# include <sys/wait.h>
# include <unistd.h>
# include <linux/sched.h>

static void
print_child_stack(const char *side)
{
	jsonl_arg_open(1, "child_stack", side);
	jsonl_addr(NULL);
	jsonl_arg_close(side);
}

static void
print_flags(const char *side)
{
	jsonl_arg_open(0, "flags", side);
	jsonl_signal(SIGCHLD, "SIGCHLD");
	printf(", \"sanitized\": true");
	jsonl_arg_close(side);
}

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "entry" : NULL;

	long pid = syscall(__NR_clone, CLONE_UNTRACED | SIGCHLD, 0, 0, 0, 0);
	if (pid < 0)
		perror_msg_and_fail("clone");
	if (!pid)
		_exit(0);

	int status;
	if (waitpid(pid, &status, 0) != pid)
		perror_msg_and_fail("waitpid");
	if (!WIFEXITED(status) || WEXITSTATUS(status))
		error_msg_and_fail("unexpected wait status %#x", status);

	if (merged)
		jsonl_syscall_merged_open("clone", __NR_clone);
	else
		jsonl_syscall_open("clone", __NR_clone, true);
	if (merged) {
		print_flags(side);
		printf(", ");
		print_child_stack(side);
	} else {
		print_child_stack(side);
		printf(", ");
		print_flags(side);
	}
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("clone", __NR_clone, false);
	}
	printf("], ");
	jsonl_return("tid", pid);
	printf("}\n");

	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_clone && (__x86_64__ || __i386__ || __aarch64__)")

#endif
