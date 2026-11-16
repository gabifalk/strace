/*
 * Check JSONL output of a killed-by-signal event.
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

int
main(void)
{
	const pid_t pid = fork();
	if (pid < 0)
		perror_msg_and_fail("fork");

	if (!pid) {
		raise(SIGKILL);
		_exit(1);
	}

	int s;
	if (waitpid(pid, &s, 0) != pid)
		perror_msg_and_fail("waitpid");

	printf("{\"event\": \"killed\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, "
	       "\"signal\": {", (int) pid);
	jsonl_signal(SIGKILL, "SIGKILL");
	printf("}, \"coredumped\": false}\n");

	return 0;
}
