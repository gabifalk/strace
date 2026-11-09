/*
 * Check JSONL output of the access syscall's flags (mode) argument.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_access

# include "jsonl.h"

# include <fcntl.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

static bool merged;
static const char *side;

static const char path[] = "strace_enoent_xyz";

static void
emit(void (*mode)(void), long rc, int err)
{
	if (merged)
		jsonl_syscall_merged_open("access", __NR_access);
	else
		jsonl_syscall_open("access", __NR_access, true);
	jsonl_arg_open(0, "pathname", side);
	printf("\"type\": \"path\", \"value\": ");
	jsonl_bytes(path);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "mode", side);
	mode();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("access", __NR_access, false);
	}
	printf("], ");
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");
}

static void
mode_f_ok(void)
{
	jsonl_flags_open(F_OK);
	jsonl_flags_const(F_OK, "F_OK", true);
	jsonl_flags_close();
}

static void
mode_rwx(void)
{
	jsonl_flags_open(R_OK | W_OK | X_OK);
	jsonl_flags_const(R_OK, "R_OK", true);
	jsonl_flags_const(W_OK, "W_OK", false);
	jsonl_flags_const(X_OK, "X_OK", false);
	jsonl_flags_close();
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	long rc1 = syscall(__NR_access, path, F_OK);
	int err1 = errno;
	long rc2 = syscall(__NR_access, path, R_OK | W_OK | X_OK);
	int err2 = errno;

	emit(mode_f_ok, rc1, err1);
	emit(mode_rwx, rc2, err2);
	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_access")

#endif
