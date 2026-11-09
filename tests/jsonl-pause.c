/*
 * Check JSONL output of the pause syscall interrupted by a signal.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_pause

# include "jsonl.h"

# include <errno.h>
# include <signal.h>
# include <stdio.h>
# include <string.h>
# include <sys/time.h>
# include <unistd.h>

# ifndef ERESTARTNOHAND
#  define ERESTARTNOHAND 514
# endif

static void
handler(int sig)
{
}

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const struct sigaction act = { .sa_handler = handler };
	if (sigaction(SIGALRM, &act, NULL))
		perror_msg_and_fail("sigaction");

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL))
		perror_msg_and_fail("sigprocmask");

	const struct itimerval itv = { .it_value.tv_usec = 123456 };
	if (setitimer(ITIMER_REAL, &itv, NULL))
		perror_msg_and_fail("setitimer");

	syscall(__NR_pause);
	if (errno == ENOSYS)
		perror_msg_and_skip("pause");

	if (merged) {
		jsonl_syscall_merged_open("pause", __NR_pause);
		printf("], ");
	} else {
		jsonl_syscall_open("pause", __NR_pause, true);
		printf("]}\n");
		jsonl_syscall_open("pause", __NR_pause, false);
		printf("], ");
	}
	printf("\"return\": {\"type\": \"interrupted\","
	       " \"errno\": {\"type\": \"errno\", \"raw\": \"%d\","
	       " \"sym\": \"ERESTARTNOHAND\","
	       " \"strerror\": \"To be restarted if no handler\"}}}\n",
	       ERESTARTNOHAND);
	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_pause")

#endif
