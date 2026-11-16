/*
 * Check JSONL output of the PIDFD_GET_INFO ioctl command that fails:
 * the struct printed on entering is split, and there is no exiting part.
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
# include <linux/pidfd.h>

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_PTR(struct pidfd_info, info);
	memset(info, 0, sizeof(*info));
	info->mask = PIDFD_INFO_PID;

	jsonl_ioctl_call(PIDFD_GET_INFO, "PIDFD_GET_INFO", NULL, info);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_struct_open();
	jsonl_field_flag("mask", true, PIDFD_INFO_PID, "PIDFD_INFO_PID");
	jsonl_struct_close();
	printf(", \"split\": true");
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
