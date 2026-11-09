/*
 * Check JSONL output of the fcntl F_GETLEASE operation.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"
#include "scno.h"

#ifdef __NR_fcntl

# include <fcntl.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

static bool merged;

static void
emit_getlease(int fd, long rc)
{
	const char *side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("fcntl", __NR_fcntl);
	else
		jsonl_syscall_open("fcntl", __NR_fcntl, true);
	jsonl_arg_open(0, "fd", side);
	jsonl_fd(fd, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "op", side);
	jsonl_const(F_GETLEASE, true, "F_GETLEASE", NULL);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("fcntl", __NR_fcntl, false);
	}
	printf("], ");
	if (rc == -1) {
		jsonl_return("unsigned", rc);
	} else {
		const char *sym = NULL;

		switch (rc) {
		case F_RDLCK: sym = "F_RDLCK"; break;
		case F_WRLCK: sym = "F_WRLCK"; break;
		case F_UNLCK: sym = "F_UNLCK"; break;
		}
		printf("\"return\": {");
		jsonl_const((uint64_t) rc, true, sym, NULL);
		printf("}");
	}
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	emit_getlease(1, syscall(__NR_fcntl, 1, F_GETLEASE));
	emit_getlease(-1, syscall(__NR_fcntl, -1, F_GETLEASE));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_fcntl")

#endif
