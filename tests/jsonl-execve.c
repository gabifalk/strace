/*
 * Check JSONL output of an environment that is not printed in full:
 * the envp argument of execve without -v (addr_count of vars).
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"
#include "jsonl.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static bool merged;
static const char *side;

static const char path[] = "/nonexistent/jsonl_execve_xyz";
static const char arg1[] = "1";

static void
test_execve(char *const *envp, unsigned int count)
{
	char *const argv[] = { (char *) path, (char *) arg1, NULL };

	long rc = syscall(__NR_execve, path, argv, envp);
	int err = errno;

	if (merged)
		jsonl_syscall_merged_open("execve", __NR_execve);
	else
		jsonl_syscall_open("execve", __NR_execve, true);
	jsonl_arg_open(0, "pathname", side);
	jsonl_path_value(path);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "argv", side);
	jsonl_array_open();
	printf("{");
	jsonl_string_value(path, sizeof(path) - 1, false);
	printf("}, {");
	jsonl_string_value(arg1, sizeof(arg1) - 1, false);
	printf("}");
	jsonl_array_close();
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "envp", side);
	printf("\"type\": \"addr_count\", \"raw\": \"%#lx\","
	       " \"count\": \"%u\", \"unit\": \"vars\"",
	       (unsigned long) (uintptr_t) envp, count);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("execve", __NR_execve, false);
	}
	printf("], ");
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	static char var[] = "A=1";
	char *const one[] = { var, NULL };
	char *const none[] = { NULL };

	test_execve(one, 1);
	test_execve(none, 0);

	jsonl_exit_event(0);
	return 0;
}
