/*
 * Check JSONL output of a truncated path argument.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_chdir

# include "jsonl.h"

# include <limits.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

int
main(int argc, char *argv[])
{
	bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *side = merged ? "entry" : NULL;

	char *const p = tail_alloc(PATH_MAX);
	memset(p, '/', PATH_MAX);

	long rc = syscall(__NR_chdir, p);
	int err = errno;

	if (merged)
		jsonl_syscall_merged_open("chdir", __NR_chdir);
	else
		jsonl_syscall_open("chdir", __NR_chdir, true);
	jsonl_arg_open(0, "path", side);
	printf("\"type\": \"path\", \"value\": ");
	jsonl_bytes_n(p, PATH_MAX - 1);
	jsonl_truncated();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("chdir", __NR_chdir, false);
	}
	printf("], ");
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_chdir")

#endif
