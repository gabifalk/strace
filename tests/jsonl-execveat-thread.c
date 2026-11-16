/*
 * Check JSONL output of the entering_pid field of an execveat syscall
 * invoked by a thread other than the thread group leader.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_execveat

# include "jsonl.h"

# include <errno.h>
# include <pthread.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>

static void *
thread(void *arg)
{
	char **const av = arg;
	char tid[sizeof(int) * 3 + 1];

	snprintf(tid, sizeof(tid), "%d", (int) syscall(__NR_gettid));

	char *const argv[] = { av[0], av[1], tid, NULL };

	syscall(__NR_execveat, -100, argv[0], argv, NULL, 0);
	perror_msg_and_skip("execveat");
}

static void
print_args(char **argv, const char *side)
{
	jsonl_arg_open(0, "dirfd", side);
	jsonl_const_decimal(-100, "AT_FDCWD", NULL);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "pathname", side);
	jsonl_path_value(argv[0]);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "argv", side);
	jsonl_array_open();
	for (unsigned int i = 0; i < 3; ++i) {
		printf("%s{", i ? ", " : "");
		jsonl_string_value(argv[i], strlen(argv[i]), false);
		printf("}");
	}
	jsonl_array_close();
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(3, "envp", side);
	jsonl_addr(NULL);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(4, "flags", side);
	jsonl_flags(0, NULL, 0, NULL);
	jsonl_arg_close(side);
}

static void
print_syscall_head(int pid, int entering_pid, const char *entering)
{
	printf("{\"event\": \"syscall\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, ", pid);
	if (entering_pid)
		printf("\"entering_pid\": {\"type\": \"tid\", "
		       "\"raw\": \"%d\"}, ", entering_pid);
	printf("\"syscall\": {\"type\": \"syscall\", \"name\": \"execveat\", "
	       "\"scno\": \"%d\", \"personality\": %d}, ",
	       __NR_execveat, JSONL_PERSONALITY);
	if (entering)
		printf("\"entering\": %s, ", entering);
	printf("\"args\": [");
}

int
main(int argc, char *argv[])
{
	if (argc < 2)
		error_msg_and_fail("missing argument");

	if (argc == 2) {
		pthread_t t;

		errno = pthread_create(&t, NULL, thread, argv);
		if (errno)
			perror_msg_and_fail("pthread_create");
		for (;;)
			pause();
	}

	const bool merged = !strcmp(argv[1], "merged");
	const int leader = getpid();
	const int tid = atoi(argv[2]);

	if (!merged) {
		print_syscall_head(tid, 0, "true");
		print_args(argv, NULL);
		printf("]}\n");
	}

	printf("{\"event\": \"superseded\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, "
	       "\"old_pid\": {\"type\": \"tid\", \"raw\": \"%d\"}}\n",
	       leader, tid);

	print_syscall_head(leader, tid, merged ? NULL : "false");
	if (merged)
		print_args(argv, "entry");
	printf("], ");
	jsonl_return("unsigned", 0);
	printf("}\n");

	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_execveat")

#endif
