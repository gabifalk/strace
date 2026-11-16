/*
 * Check JSONL output of the stack field of syscall and signal events.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_chdir && defined __NR_kill

# include "jsonl.h"
# include "stack-fcall.h"

# include <limits.h>
# include <signal.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

static char exe[PATH_MAX];

static void
print_stack(void)
{
	static const char *const syms[] = { "f3", "f2", "f1", "f0", "main" };

	printf(", \"stack\": {\"frames\": [");
	for (unsigned int i = 0; i < ARRAY_SIZE(syms); ++i)
		printf("%s{\"binary\": \"%s\", \"symbol\": \"%s\", "
		       "\"offset\": \"OFF\", \"address\": \"ADDR\"}",
		       i ? ", " : "", exe, syms[i]);
	printf("]}");
}

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "entry" : NULL;

	const ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
	if (len < 0)
		perror_msg_and_skip("readlink");
	exe[len] = '\0';

	f0(1, (unsigned long) (void *) main);

	if (merged)
		jsonl_syscall_merged_open("chdir", __NR_chdir);
	else
		jsonl_syscall_open("chdir", __NR_chdir, true);
	jsonl_arg_open(0, "path", side);
	jsonl_path_value("");
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("chdir", __NR_chdir, false);
	}
	printf("], ");
	jsonl_return("decimal", -1);
	print_stack();
	printf("}\n");

	f0(2, (unsigned long) (void *) main);

	printf("{\"event\": \"signal\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, \"signal\": {",
	       (int) getpid());
	jsonl_signal(SIGURG, "SIGURG");
	printf("}, \"info\": {\"type\": \"struct\", \"fields\": {"
	       "\"si_signo\": {");
	jsonl_signal(SIGURG, "SIGURG");
	printf("}, \"si_code\": {");
	jsonl_const(SI_USER, false, "SI_USER", NULL);
	printf("}, \"si_pid\": {\"type\": \"tgid\", \"raw\": \"%d\"}, "
	       "\"si_uid\": {\"type\": \"uid\", \"raw\": \"%d\"}}}",
	       (int) getpid(), (int) geteuid());
	print_stack();
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_chdir && __NR_kill")

#endif
