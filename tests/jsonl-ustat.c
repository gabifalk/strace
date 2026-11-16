/*
 * Check JSONL output of the ustat syscall's dev_t argument.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_ustat

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/sysmacros.h>

static bool merged;

static void
emit(unsigned int dev, long rc, int err)
{
	if (merged)
		jsonl_syscall_merged_open("ustat", __NR_ustat);
	else
		jsonl_syscall_open("ustat", __NR_ustat, true);

	jsonl_arg_open(0, "dev", merged ? "entry" : NULL);
	jsonl_dev_t(dev);
	jsonl_arg_close(merged ? "entry" : NULL);

	if (merged) {
		printf(", ");
	} else {
		printf("]}\n");
		jsonl_syscall_open("ustat", __NR_ustat, false);
	}
	jsonl_arg_open(1, "ubuf", merged ? "exit" : NULL);
	printf("\"type\": \"addr\", \"raw\": null");
	jsonl_arg_close(merged ? "exit" : NULL);

	printf("], ");
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");
}

static void
test_dev(kernel_ulong_t dev)
{
	long rc = syscall(__NR_ustat, dev, 0);
	emit((unsigned int) dev, rc, errno);
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	test_dev(0);
	test_dev((kernel_ulong_t) 0xfacefeedffffffffULL);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ustat")

#endif
