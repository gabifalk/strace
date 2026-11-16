/*
 * Check JSONL output of pit_count values: the argument
 * of KIOCSOUND ioctl command.
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
test_sound(unsigned int count, unsigned int freq)
{
	jsonl_ioctl_call(KIOCSOUND, "KIOCSOUND", NULL,
			 (void *) (uintptr_t) count);
	jsonl_ioctl_argp_open("argp", false);
	printf("\"type\": \"pit_count\", \"raw\": \"%u\", \"freq_hz\": \"%u\"",
	       count, freq);
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();
}

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	test_sound(4242, 281);
	test_sound(0, 0);
	test_sound(1193183, 0);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
