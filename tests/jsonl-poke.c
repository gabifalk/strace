/*
 * Check JSONL output of poke injection: the poked arguments carry
 * "injected": true, and so does a return value injected with them.
 * The test is run with dup's oldfd poked to 100, and with dup2's newfd
 * poked to 101 and its return value injected as 42.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_dup && defined __NR_dup2

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>

static bool merged;
static const char *side;

static void
open_event(const char *name, long scno)
{
	if (merged)
		jsonl_syscall_merged_open(name, scno);
	else
		jsonl_syscall_open(name, scno, true);
}

static void
print_fd_arg(unsigned int index, const char *name, int fd, bool injected)
{
	jsonl_arg_open(index, name, side);
	if (injected)
		printf("\"injected\": true, ");
	jsonl_fd(fd, NULL, false);
	jsonl_arg_close(side);
}

static void
open_return(const char *name, long scno)
{
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open(name, scno, false);
	}
	printf("], ");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	long rc = syscall(__NR_dup, 42);
	open_event("dup", __NR_dup);
	print_fd_arg(0, "oldfd", 100, true);
	open_return("dup", __NR_dup);
	jsonl_return("decimal", rc);
	printf("}\n");

	rc = syscall(__NR_dup2, 42, 43);
	open_event("dup2", __NR_dup2);
	print_fd_arg(0, "oldfd", 42, false);
	printf(", ");
	print_fd_arg(1, "newfd", 101, true);
	open_return("dup2", __NR_dup2);
	printf("\"return\": {\"type\": \"fd\", \"raw\": \"%ld\","
	       " \"injected\": true}}\n", rc);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_dup && __NR_dup2")

#endif
