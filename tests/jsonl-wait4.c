/*
 * Check JSONL output of wait4 syscall: the status argument is
 * a wait_status value of an exited, signaled, stopped, or continued child.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_wait4

# include "jsonl.h"

# include <signal.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/wait.h>

static bool merged;

static void
test_wait4(const pid_t pid, const int options, const char *const options_sym,
	   const char *const sig_sym)
{
	const char *const entry = merged ? "entry" : NULL;
	const char *const exit = merged ? "exit" : NULL;
	int status;

	long rc = syscall(__NR_wait4, pid, &status, options, 0);
	if (rc != pid)
		perror_msg_and_fail("wait4");

	if (merged)
		jsonl_syscall_merged_open("wait4", __NR_wait4);
	else
		jsonl_syscall_open("wait4", __NR_wait4, true);
	jsonl_arg_open(0, "pid", entry);
	jsonl_pid("tgid", pid);
	jsonl_arg_close(entry);
	if (merged) {
		printf(", ");
	} else {
		printf("]}\n");
		jsonl_syscall_open("wait4", __NR_wait4, false);
	}

	jsonl_arg_open(1, "status", exit);
	printf("\"type\": \"wait_status\", \"raw\": \"%#x\"", status);
	if (WIFEXITED(status)) {
		printf(", \"status\": \"exited\", \"exit_code\": \"%d\"",
		       WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		printf(", \"status\": \"signaled\", \"signal\": {");
		jsonl_signal(WTERMSIG(status), sig_sym);
		printf("}");
	} else if (WIFSTOPPED(status)) {
		printf(", \"status\": \"stopped\", \"signal\": {");
		jsonl_signal(WSTOPSIG(status), sig_sym);
		printf("}");
	} else if (WIFCONTINUED(status)) {
		printf(", \"status\": \"continued\"");
	} else {
		error_msg_and_fail("unexpected status %#x", status);
	}
	jsonl_indirect();
	jsonl_arg_close(exit);
	printf(", ");
	jsonl_arg_open(2, "options", exit);
	jsonl_flags_open(options);
	if (options_sym)
		jsonl_flags_const(options, options_sym, true);
	jsonl_flags_close();
	jsonl_arg_close(exit);
	printf(", ");
	jsonl_arg_open(3, "usage", exit);
	jsonl_addr(NULL);
	jsonl_arg_close(exit);
	printf("], ");
	jsonl_return("tgid", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	pid_t pid = fork();
	if (pid < 0)
		perror_msg_and_fail("fork");
	if (!pid)
		_exit(42);
	test_wait4(pid, 0, NULL, NULL);

	pid = fork();
	if (pid < 0)
		perror_msg_and_fail("fork");
	if (!pid) {
		raise(SIGUSR1);
		_exit(1);
	}
	test_wait4(pid, 0, NULL, "SIGUSR1");

	pid = fork();
	if (pid < 0)
		perror_msg_and_fail("fork");
	if (!pid) {
		for (;;)
			pause();
	}
	if (kill(pid, SIGSTOP))
		perror_msg_and_fail("kill");
	test_wait4(pid, WSTOPPED, "WSTOPPED", "SIGSTOP");
	if (kill(pid, SIGCONT))
		perror_msg_and_fail("kill");
	test_wait4(pid, WCONTINUED, "WCONTINUED", NULL);
	if (kill(pid, SIGKILL))
		perror_msg_and_fail("kill");
	test_wait4(pid, 0, NULL, "SIGKILL");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_wait4")

#endif
