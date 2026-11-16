/*
 * Check JSONL output of the VIDIOC_QUERYCAP ioctl command: the version
 * field of struct v4l2_capability is a kernel_version value.
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
# include <linux/videodev2.h>

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_PTR(struct v4l2_capability, cap);
	memset(cap, 0, sizeof(*cap));
	memcpy(cap->driver, "drv", 3);
	cap->version = (6 << 16) | (10 << 8) | 3;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE;

	jsonl_ioctl_call(VIDIOC_QUERYCAP, "VIDIOC_QUERYCAP", NULL, cap);
	jsonl_ioctl_argp_open("argp", true);
	jsonl_struct_open();
	jsonl_field_string("driver", true, "drv");
	jsonl_field_string("card", false, "");
	jsonl_field_string("bus_info", false, "");
	printf(", \"version\": {\"type\": \"kernel_version\", \"raw\": \"%u\","
	       " \"major\": \"6\", \"minor\": \"10\", \"patch\": \"3\"}",
	       cap->version);
	jsonl_field_flag("capabilities", false, V4L2_CAP_VIDEO_CAPTURE,
			 "V4L2_CAP_VIDEO_CAPTURE");
	jsonl_struct_close();
	jsonl_ioctl_argp_close();
	printf("], ");
	jsonl_return_injected("unsigned", jsonl_ioctl_rc);
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
