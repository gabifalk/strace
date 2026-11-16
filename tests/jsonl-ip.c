/*
 * Check JSONL output of the event-level ip field.
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

# include <stdio.h>
# include <string.h>
# include <unistd.h>

static void
print_event_head(const char *event, const char *ip)
{
	printf("{\"event\": \"%s\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, "
	       "\"ip\": \"%s\"", event, (int) getpid(), ip);
}

static void
print_syscall_head(bool merged, bool entering)
{
	print_event_head("syscall", "IP");
	printf(", \"syscall\": {\"type\": \"syscall\", \"name\": \"chdir\", "
	       "\"scno\": \"%d\", \"personality\": %d}",
	       __NR_chdir, JSONL_PERSONALITY);
	if (!merged)
		printf(", \"entering\": %s", entering ? "true" : "false");
	printf(", \"args\": [");
}

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "entry" : NULL;

	const long rc = syscall(__NR_chdir, ".");

	print_syscall_head(merged, true);
	jsonl_arg_open(0, "path", side);
	jsonl_path_value(".");
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		print_syscall_head(merged, false);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");

	print_event_head("exit", sizeof(long) == 4 ? "????????"
						   : "????????????????");
	printf(", \"status\": 0}\n");
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_chdir")

#endif
