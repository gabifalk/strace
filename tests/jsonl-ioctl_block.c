/*
 * Check JSONL output of the BLKDISCARD ioctl command: its argument
 * is a pair of numbers read from tracee memory.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_ioctl

# include "jsonl_ioctl.h"

# include <inttypes.h>
# include <stdio.h>
# include <string.h>
# include <linux/fs.h>

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_ARR(uint64_t, pair, 2);
	pair[0] = 0xdeadbeefbadc0dedULL;
	pair[1] = 0xfacefeedcafef00dULL;

	jsonl_ioctl_call(BLKDISCARD, "BLKDISCARD", NULL, pair);
	jsonl_ioctl_argp_open("argp", false);
	printf("\"type\": \"pair\", \"elems\": [{");
	jsonl_unsigned(pair[0]);
	printf("}, {");
	jsonl_unsigned(pair[1]);
	printf("}]");
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
