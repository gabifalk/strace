/*
 * Check JSONL output of a signal-delivery event and the group-stop event
 * that follows it.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int
main(void)
{
	const pid_t pid = fork();
	if (pid < 0)
		perror_msg_and_fail("fork");

	if (!pid) {
		raise(SIGSTOP);
		_exit(0);
	}

	int s;
	if (waitpid(pid, &s, WUNTRACED) != pid)
		perror_msg_and_fail("waitpid WUNTRACED");

	/* The self-raised SIGSTOP delivered to the child (SI_TKILL). */
	printf("{\"event\": \"signal\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, "
	       "\"signal\": {", (int) pid);
	jsonl_signal(SIGSTOP, "SIGSTOP");
	printf("}, \"info\": {\"type\": \"struct\", \"fields\": {"
	       "\"si_signo\": {");
	jsonl_signal(SIGSTOP, "SIGSTOP");
	printf("}, \"si_code\": {");
	jsonl_const((uint32_t) SI_TKILL, true, "SI_TKILL", NULL);
	printf("}, \"si_pid\": {\"type\": \"decimal\", \"raw\": \"%d\"}, "
	       "\"si_uid\": {\"type\": \"uid\", \"raw\": \"%d\"}}}}\n",
	       (int) pid, (int) getuid());

	/* The resulting group-stop. */
	printf("{\"event\": \"stopped\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, "
	       "\"signal\": {", (int) pid);
	jsonl_signal(SIGSTOP, "SIGSTOP");
	printf("}}\n");

	if (kill(pid, SIGCONT))
		perror_msg_and_fail("kill SIGCONT");
	if (waitpid(pid, &s, 0) != pid)
		perror_msg_and_fail("waitpid");

	return 0;
}
