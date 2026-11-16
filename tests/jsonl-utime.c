/*
 * Check JSONL output of the utime syscall's time_t arguments.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_utime

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <time.h>
# include <unistd.h>
# include <utime.h>

static bool merged;
static const char *side;

static const time_t t = 1492350678;
static const char path[] = "utime\nfilename";

static void
emit(long rc, int err)
{
	if (merged)
		jsonl_syscall_merged_open("utime", __NR_utime);
	else
		jsonl_syscall_open("utime", __NR_utime, true);

	jsonl_arg_open(0, "filename", side);
	printf("\"type\": \"path\", \"value\": ");
	jsonl_bytes(path);
	jsonl_arg_close(side);

	printf(", ");
	jsonl_arg_open(1, "times", side);
	printf("\"type\": \"struct\", \"fields\": {\"actime\": {");
	jsonl_time_t((long long) t);
	printf("}, \"modtime\": {");
	jsonl_time_t((long long) t);
	printf("}}");
	jsonl_arg_close(side);

	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("utime", __NR_utime, false);
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

	const struct utimbuf u = { .actime = t, .modtime = t };
	long rc = syscall(__NR_utime, path, &u);
	int err = errno;

	emit(rc, err);
	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_utime")

#endif
