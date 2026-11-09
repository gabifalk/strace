/*
 * Check JSONL output of a SIGCHLD signal-delivery event, including the
 * ticks-valued si_utime and si_stime fields of the siginfo info struct.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"

#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

static siginfo_t sinfo;

static void
handler(int no, siginfo_t *si, void *uc)
{
	sinfo = *si;
}

int
main(void)
{
	int fds[2];
	if (pipe(fds))
		perror_msg_and_fail("pipe");

	const pid_t pid = fork();
	if (pid < 0)
		perror_msg_and_fail("fork");

	if (!pid) {
		/* Wait for the parent, then exit without consuming CPU time. */
		char c;
		(void) close(fds[1]);
		if (read(fds[0], &c, sizeof(c)) != 1)
			_exit(1);
		_exit(42);
	}

	(void) close(fds[0]);

	const struct sigaction act = {
		.sa_sigaction = handler,
		.sa_flags = SA_SIGINFO
	};
	if (sigaction(SIGCHLD, &act, NULL))
		perror_msg_and_fail("sigaction");

	sigset_t mask, oldmask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	if (sigprocmask(SIG_BLOCK, &mask, &oldmask))
		perror_msg_and_fail("sigprocmask");

	/* Release the child and wait for the SIGCHLD delivery. */
	if (write(fds[1], "", 1) != 1)
		perror_msg_and_fail("write");
	(void) close(fds[1]);

	sigset_t unblock = oldmask;
	sigdelset(&unblock, SIGCHLD);
	sigsuspend(&unblock);

	printf("{\"event\": \"signal\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, "
	       "\"signal\": {", (int) getpid());
	jsonl_signal(SIGCHLD, "SIGCHLD");
	printf("}, \"info\": {\"type\": \"struct\", \"fields\": {"
	       "\"si_signo\": {");
	jsonl_signal(SIGCHLD, "SIGCHLD");
	printf("}, \"si_code\": {");
	jsonl_const(CLD_EXITED, true, "CLD_EXITED", NULL);
	printf("}, \"si_pid\": {\"type\": \"decimal\", \"raw\": \"%d\"}, "
	       "\"si_uid\": {\"type\": \"uid\", \"raw\": \"%d\"}, "
	       "\"si_status\": {\"type\": \"decimal\", \"raw\": \"%d\"}, "
	       "\"si_utime\": {",
	       (int) sinfo.si_pid, (int) sinfo.si_uid, sinfo.si_status);
	jsonl_ticks(zero_extend_signed_to_ull(sinfo.si_utime));
	printf("}, \"si_stime\": {");
	jsonl_ticks(zero_extend_signed_to_ull(sinfo.si_stime));
	printf("}}}}\n");

	int s;
	if (waitpid(pid, &s, 0) != pid)
		perror_msg_and_fail("waitpid");

	jsonl_exit_event(0);
	return 0;
}
