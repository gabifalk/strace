/*
 * Check JSONL output of signal numbers decoded by printsignal.
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

# include <signal.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

static bool merged;

static void
emit_kill(pid_t pid, int sig, const char *sym, long rc)
{
	const char *side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("kill", __NR_kill);
	else
		jsonl_syscall_open("kill", __NR_kill, true);
	jsonl_arg_open(0, "pid", side);
	printf("\"type\": \"decimal\", \"raw\": \"%d\"", (int) pid);
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
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	const pid_t pid = getpid();

	emit_kill(pid, SIGCONT, "SIGCONT",
		  syscall(__NR_kill, pid, (long) SIGCONT));
	emit_kill(pid, 0, NULL, syscall(__NR_kill, pid, (long) 0));
	emit_kill(pid, 256, NULL, syscall(__NR_kill, pid, (long) 256));
	emit_kill(pid, -1, NULL, syscall(__NR_kill, pid, (long) -1));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_kill")

#endif
