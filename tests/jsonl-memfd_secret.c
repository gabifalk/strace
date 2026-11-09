/*
 * Check JSONL output of the memfd_secret syscall's flags argument.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_memfd_secret

# include "jsonl.h"

# include <fcntl.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

static bool merged;
static const char *side;

static void
emit(void (*flags)(void), long rc, int err)
{
	if (merged)
		jsonl_syscall_merged_open("memfd_secret", __NR_memfd_secret);
	else
		jsonl_syscall_open("memfd_secret", __NR_memfd_secret, true);
	jsonl_arg_open(0, "flags", side);
	flags();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("memfd_secret", __NR_memfd_secret, false);
	}
	printf("], ");
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");
}

static void
flags_cloexec(void)
{
	jsonl_flags_open(O_CLOEXEC | 0x10);
	jsonl_flags_const(O_CLOEXEC, "O_CLOEXEC", true);
	jsonl_flags_remainder(0x10, NULL, false);
	jsonl_flags_close();
}

static void
flags_empty_dflt(void)
{
	jsonl_flags_open(0x10);
	jsonl_flags_remainder(0x10, "", true);
	jsonl_flags_close();
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	long rc1 = syscall(__NR_memfd_secret, O_CLOEXEC | 0x10);
	int err1 = errno;
	long rc2 = syscall(__NR_memfd_secret, 0x10);
	int err2 = errno;

	emit(flags_cloexec, rc1, err1);
	emit(flags_empty_dflt, rc2, err2);
	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_memfd_secret")

#endif
