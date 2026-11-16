/*
 * Check JSONL output of BTRFS_IOC_DEV_INFO command of ioctl syscall.
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
# include <linux/btrfs.h>

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	static const unsigned char uuid[BTRFS_UUID_SIZE] = {
		0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
		0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
	};
	TAIL_ALLOC_OBJECT_CONST_PTR(struct btrfs_ioctl_dev_info_args, args);

	memset(args, 0, sizeof(*args));
	args->devid = 1;
	memcpy(args->uuid, uuid, sizeof(uuid));

	jsonl_ioctl_call(ARG_STR(BTRFS_IOC_DEV_INFO), NULL, args);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_struct_open();
	jsonl_field_open("devid", true);
	jsonl_dev_t(1);
	jsonl_field_close();
	printf(", \"uuid\": {\"type\": \"uuid\","
	       " \"value\": \"01234567-89ab-cdef-fedc-ba9876543210\"}");
	jsonl_struct_close();
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
