/*
 * Check JSONL output of kd_tone values: the argument
 * of KDMKTONE ioctl command.
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

# include <stdio.h>
# include <string.h>
# include <linux/kd.h>

static void
test_tone(unsigned int ticks, unsigned int count, unsigned int freq)
{
	const unsigned int arg = ticks << 16 | count;

	jsonl_ioctl_call(KDMKTONE, "KDMKTONE", NULL, (void *) (uintptr_t) arg);
	jsonl_ioctl_argp_open("argp", false);
	printf("\"type\": \"kd_tone\", \"raw\": \"%#x\", \"ticks\": \"%u\","
	       " \"count\": \"%u\", \"freq_hz\": \"%u\"",
	       arg, ticks, count, freq);
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();
}

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	test_tone(100, 4242, 281);
	test_tone(0, 440, 0);
	test_tone(1, 0, 0);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
