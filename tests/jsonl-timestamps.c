/*
 * Check JSONL output of the event-level timestamp and relative_timestamp
 * fields.  The test is run with -tt -r, and its seconds and nanoseconds
 * are replaced with N.
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
print_event_head(const char *event)
{
	printf("{\"event\": \"%s\", \"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, "
	       "\"timestamp\": {\"type\": \"time\", \"seconds\": \"N\", "
	       "\"nanoseconds\": \"N\", \"precision\": \"6\"}, "
	       "\"relative_timestamp\": {\"type\": \"duration\", "
	       "\"seconds\": \"N\", \"nanoseconds\": \"N\", "
	       "\"precision\": \"6\"}", event, (int) getpid());
}

static void
print_syscall_head(bool entering)
{
	print_event_head("syscall");
	printf(", \"syscall\": {\"type\": \"syscall\", \"name\": \"chdir\", "
	       "\"scno\": \"%d\", \"personality\": %d}",
	       __NR_chdir, JSONL_PERSONALITY);
	printf(", \"entering\": %s, \"args\": [",
	       entering ? "true" : "false");
}

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "entry" : NULL;

	const long rc = syscall(__NR_chdir, ".");

	if (merged) {
		print_event_head("syscall");
		printf(", \"syscall\": {\"type\": \"syscall\", "
		       "\"name\": \"chdir\", \"scno\": \"%d\", "
		       "\"personality\": %d}, \"args\": [",
		       __NR_chdir, JSONL_PERSONALITY);
	} else {
		print_syscall_head(true);
	}
	jsonl_arg_open(0, "path", side);
	printf("\"type\": \"path\", \"value\": ");
	jsonl_bytes(".");
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		print_syscall_head(false);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");

	print_event_head("exit");
	printf(", \"status\": 0}\n");
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_chdir")

#endif
