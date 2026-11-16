/*
 * Check JSONL output of kd_io_port values: the argument
 * of KDADDIO and KDDELIO ioctl commands.
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
test_io(unsigned int code, const char *name, unsigned int port,
	int gp_index)
{
	jsonl_ioctl_call(code, name, NULL, (void *) (uintptr_t) port);
	jsonl_ioctl_argp_open("argp", false);
	printf("\"type\": \"kd_io_port\", \"raw\": \"%#x\"", port);
	if (gp_index >= 0)
		printf(", \"gp_index\": \"%d\"", gp_index);
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();
}

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	test_io(ARG_STR(KDADDIO), 0x3b5, 1);
	test_io(ARG_STR(KDDELIO), 0x3df, 43);
	test_io(ARG_STR(KDADDIO), 0x3b3, -1);
	test_io(ARG_STR(KDDELIO), 0x3e0, -1);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
