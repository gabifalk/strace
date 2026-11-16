/*
 * Check JSONL output of rt_sigtimedwait: the timeout saved on entering
 * and printed on exiting, and the signal return value.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_rt_sigtimedwait

# include "jsonl.h"
# include "kernel_old_timespec.h"

# include <signal.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const entry = merged ? "entry" : NULL;
	const char *const exit_side = merged ? "exit" : NULL;
	const unsigned int sigsetsize = _NSIG / 8;

	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	if (sigprocmask(SIG_BLOCK, &set, NULL))
		perror_msg_and_fail("sigprocmask");
	if (kill(getpid(), SIGUSR1))
		perror_msg_and_fail("kill");

	siginfo_t info;
	memset(&info, 0, sizeof(info));
	const kernel_old_timespec_t ts = { .tv_sec = 1, .tv_nsec = 2 };
	const long rc = syscall(__NR_rt_sigtimedwait, &set, &info, &ts,
				sigsetsize);

	if (merged)
		jsonl_syscall_merged_open("rt_sigtimedwait",
					  __NR_rt_sigtimedwait);
	else
		jsonl_syscall_open("rt_sigtimedwait", __NR_rt_sigtimedwait,
				   true);
	jsonl_arg_open(0, "set", entry);
	printf("\"type\": \"bitset\", \"raw\": \"%#x\", \"elems\": [{",
	       1U << (SIGUSR1 - 1));
	jsonl_signal(SIGUSR1, "SIGUSR1");
	printf("}]");
	jsonl_arg_close(entry);
	if (merged) {
		printf(", ");
	} else {
		printf("]}\n");
		jsonl_syscall_open("rt_sigtimedwait", __NR_rt_sigtimedwait,
				   false);
	}
	jsonl_arg_open(1, "info", exit_side);
	printf("\"type\": \"struct\", \"fields\": {\"si_signo\": {");
	jsonl_signal(SIGUSR1, "SIGUSR1");
	printf("}, \"si_code\": {");
	jsonl_const(SI_USER, false, "SI_USER", NULL);
	printf("}, \"si_pid\": {\"type\": \"tgid\", \"raw\": \"%d\"}, "
	       "\"si_uid\": {\"type\": \"uid\", \"raw\": \"%u\"}}",
	       (int) info.si_pid, (unsigned int) info.si_uid);
	jsonl_arg_close(exit_side);
	printf(", ");
	jsonl_arg_open(2, "timeout", exit_side);
	jsonl_timespec(ts.tv_sec, ts.tv_nsec);
	jsonl_arg_close(exit_side);
	printf(", ");
	jsonl_arg_open(3, "sigsetsize", exit_side);
	jsonl_unsigned(sigsetsize);
	jsonl_arg_close(exit_side);
	printf("], \"return\": {");
	jsonl_signal(rc, "SIGUSR1");
	printf("}}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_rt_sigtimedwait")

#endif
