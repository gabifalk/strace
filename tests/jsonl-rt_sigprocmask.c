/*
 * Check JSONL output of signal masks: the set argument of rt_sigprocmask.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"
#include "jsonl.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static bool merged;

static const char *
sig_name(unsigned int sig)
{
	switch (sig) {
	case SIGKILL: return "SIGKILL";
	case SIGUSR2: return "SIGUSR2";
	case SIGCHLD: return "SIGCHLD";
	default: error_msg_and_fail("unexpected signal %u", sig);
	}
}

static void
print_mask(const uint32_t mask[2], bool inverted)
{
	printf("\"type\": \"bitset\", \"raw\": ");
	if (mask[1])
		printf("\"%#x%08x\"", mask[1], mask[0]);
	else
		printf("\"%#x\"", mask[0]);
	if (inverted)
		printf(", \"inverted\": true");
	printf(", \"elems\": [");

	bool first = true;
	for (unsigned int i = 0; i < 64; ++i) {
		const bool set = mask[i / 32] & (1U << (i % 32));

		if (set == inverted)
			continue;
		printf("%s{", first ? "" : ", ");
		jsonl_signal(i + 1, sig_name(i + 1));
		printf("}");
		first = false;
	}
	printf("]");
}

static void
test_sigprocmask(int how, const char *how_sym, const uint32_t mask[2],
		 bool inverted)
{
	long rc = syscall(__NR_rt_sigprocmask, how, mask, NULL, 8);

	if (rc)
		perror_msg_and_skip("rt_sigprocmask");

	if (merged)
		jsonl_syscall_merged_open("rt_sigprocmask", __NR_rt_sigprocmask);
	else
		jsonl_syscall_open("rt_sigprocmask", __NR_rt_sigprocmask, true);
	jsonl_arg_open(0, "how", merged ? "entry" : NULL);
	jsonl_const(how, true, how_sym, NULL);
	jsonl_arg_close(merged ? "entry" : NULL);
	printf(", ");
	jsonl_arg_open(1, "set", merged ? "entry" : NULL);
	print_mask(mask, inverted);
	jsonl_arg_close(merged ? "entry" : NULL);
	if (merged) {
		printf(", ");
	} else {
		printf("]}\n");
		jsonl_syscall_open("rt_sigprocmask", __NR_rt_sigprocmask,
				   false);
	}
	jsonl_arg_open(2, "oldset", merged ? "exit" : NULL);
	jsonl_addr(NULL);
	jsonl_arg_close(merged ? "exit" : NULL);
	printf(", ");
	jsonl_arg_open(3, "sigsetsize", merged ? "exit" : NULL);
	jsonl_unsigned(8);
	jsonl_arg_close(merged ? "exit" : NULL);
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	static const uint32_t empty[2];
	static const uint32_t some[2] = {
		(1U << (SIGUSR2 - 1)) | (1U << (SIGCHLD - 1))
	};
	static const uint32_t all_but_kill[2] = {
		~(1U << (SIGKILL - 1)), ~0U
	};

	test_sigprocmask(SIG_SETMASK, "SIG_SETMASK", empty, false);
	test_sigprocmask(SIG_BLOCK, "SIG_BLOCK", some, false);
	test_sigprocmask(SIG_BLOCK, "SIG_BLOCK", all_but_kill, true);
	test_sigprocmask(SIG_SETMASK, "SIG_SETMASK", empty, false);

	jsonl_exit_event(0);
	return 0;
}
