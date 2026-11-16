/*
 * Check JSONL output of the pid argument of kill.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"
#include "scno.h"

#ifdef __NR_kill

# include <errno.h>
# include <signal.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

static bool merged;

static void
handler(int sig)
{
}

static void
emit_kill(const char *pid_type, int pid, int sig, const char *sym, long rc)
{
	const char *side = merged ? "entry" : NULL;
	int err = errno;

	if (merged)
		jsonl_syscall_merged_open("kill", __NR_kill);
	else
		jsonl_syscall_open("kill", __NR_kill, true);
	jsonl_arg_open(0, "pid", side);
	jsonl_pid(pid_type, pid);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "signum", side);
	jsonl_signal(sig, sym);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("kill", __NR_kill, false);
	}
	printf("], ");
	errno = err;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	const struct sigaction act = { .sa_handler = handler };
	if (sigaction(SIGALRM, &act, NULL))
		perror_msg_and_fail("sigaction");

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL))
		perror_msg_and_fail("sigprocmask");

	const int pid = getpid();
	long rc = syscall(__NR_kill, pid, (long) 0xdefaced00000000ULL | SIGALRM);
	emit_kill("tgid", pid, SIGALRM, "SIGALRM", rc);

	const long big_pid = (long) 0xfacefeedbadc0dedULL;
	const long big_sig = (long) 0xdeadbeefcafef00dULL;
	rc = syscall(__NR_kill, big_pid, big_sig);
	emit_kill("pgid", (int) big_pid, (int) big_sig, NULL, rc);

	rc = syscall(__NR_kill, 0, 0);
	emit_kill("decimal", 0, 0, NULL, rc);

	rc = syscall(__NR_kill, -1, 0);
	emit_kill("decimal", -1, 0, NULL, rc);

	rc = syscall(__NR_kill, (long) 0xdefaced00000000ULL | pid, 0);
	emit_kill("tgid", pid, 0, NULL, rc);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_kill")

#endif
