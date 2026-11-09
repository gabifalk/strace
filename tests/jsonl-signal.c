/*
 * Check JSONL output of signal-delivery events decoded by printsiginfo.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_kill

# include "jsonl.h"

# include <signal.h>
# include <stdio.h>
# include <unistd.h>

static void
handler(int sig, siginfo_t *info, void *ucontext)
{
}

static void
emit_signal(pid_t pid, int sig, const char *sym, int uid)
{
	printf("{\"event\": \"signal\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, "
	       "\"signal\": {", (int) pid);
	jsonl_signal(sig, sym);
	printf("}, \"info\": {\"type\": \"struct\", \"fields\": {"
	       "\"si_signo\": {");
	jsonl_signal(sig, sym);
	printf("}, \"si_code\": {");
	jsonl_const(SI_USER, false, "SI_USER", NULL);
	printf("}, \"si_pid\": {\"type\": \"decimal\", \"raw\": \"%d\"}, "
	       "\"si_uid\": {\"type\": \"uid\", \"raw\": \"%d\"}}}}\n",
	       (int) pid, uid);
}

int
main(void)
{
	static const struct {
		int sig;
		const char *sym;
	} signals[] = {
		{ SIGHUP,  "SIGHUP"  },
		{ SIGUSR1, "SIGUSR1" },
		{ SIGUSR2, "SIGUSR2" },
	};

	const struct sigaction act = {
		.sa_sigaction = handler,
		.sa_flags = SA_SIGINFO
	};
	const pid_t pid = getpid();
	const int uid = geteuid();

	for (unsigned int i = 0; i < ARRAY_SIZE(signals); ++i) {
		const int sig = signals[i].sig;

		if (sigaction(sig, &act, NULL))
			perror_msg_and_fail("sigaction: %d", sig);

		sigset_t mask;
		sigemptyset(&mask);
		sigaddset(&mask, sig);
		if (sigprocmask(SIG_UNBLOCK, &mask, NULL))
			perror_msg_and_fail("sigprocmask: %d", sig);

		if (syscall(__NR_kill, pid, (long) sig))
			perror_msg_and_fail("kill: %d", sig);

		emit_signal(pid, sig, signals[i].sym, uid);
	}

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_kill")

#endif
