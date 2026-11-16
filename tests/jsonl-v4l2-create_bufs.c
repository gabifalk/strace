/*
 * Check JSONL output of the VIDIOC_CREATE_BUFS result return value:
 * the index and count fields written back by the kernel.  The test is
 * run with ioctl returning an injected 0.
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

	TAIL_ALLOC_OBJECT_CONST_PTR(struct v4l2_create_buffers, cbuf);
	memset(cbuf, 0, sizeof(*cbuf));
	cbuf->index = 3;
	cbuf->count = 2;
	cbuf->memory = V4L2_MEMORY_MMAP;
	cbuf->format.type = 0xdead;

	jsonl_ioctl_call(VIDIOC_CREATE_BUFS, "VIDIOC_CREATE_BUFS", NULL, cbuf);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_struct_open();
	jsonl_field_unsigned("count", true, 2);
	jsonl_field_const("memory", false, V4L2_MEMORY_MMAP,
			  "V4L2_MEMORY_MMAP", NULL);
	printf(", \"format\": {");
	jsonl_struct_open();
	printf("\"type\": {");
	jsonl_const(0xdead, true, NULL, "V4L2_BUF_TYPE_???");
	printf("}");
	jsonl_struct_close();
	printf("}");
	jsonl_struct_close();
	jsonl_ioctl_argp_close();

	if (!jsonl_ioctl_merged) {
		printf("]}\n");
		jsonl_syscall_open("ioctl", __NR_ioctl, false);
	}
	printf("], \"return\": {\"type\": \"v4l2_create_buffers_result\","
	       " \"raw\": \"%ld\", \"argp\": {", jsonl_ioctl_rc);
	jsonl_struct_open();
	jsonl_field_unsigned("index", true, cbuf->index);
	jsonl_field_unsigned("count", false, cbuf->count);
	jsonl_struct_close();
	printf("}, \"injected\": true}}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
