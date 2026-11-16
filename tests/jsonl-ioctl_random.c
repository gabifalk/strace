/*
 * Check JSONL output of RND* commands of ioctl syscall.
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
# include <sys/ioctl.h>
# include <linux/ioctl.h>
# include <linux/types.h>
# include <linux/random.h>

# define XLAT_MACROS_ONLY
# include "xlat/random_ioctl_cmds.h"
# undef XLAT_MACROS_ONLY

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	union {
		char c[sizeof(struct rand_pool_info) + 8];
		struct rand_pool_info info;
	} u;
	struct rand_pool_info *info = &u.info;
	int cnt = 6;

	memcpy(info->buf, "12345678", 8);
	info->buf_size = 8;
	info->entropy_count = 3;

	jsonl_ioctl_call(ARG_STR(RNDGETENTCNT), NULL, &cnt);
	jsonl_ioctl_argp_open("argp", true);
	jsonl_addr(&cnt);
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_ioctl_call(ARG_STR(RNDADDTOENTCNT), NULL, &cnt);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_decimal(cnt);
	jsonl_indirect();
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_ioctl_call(ARG_STR(RNDADDENTROPY), NULL, NULL);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_addr(NULL);
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_ioctl_call(ARG_STR(RNDADDENTROPY), NULL, info);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_struct_open();
	jsonl_field_decimal("entropy_count", true, info->entropy_count);
	jsonl_field_decimal("buf_size", false, info->buf_size);
	jsonl_field_open("buf", false);
	jsonl_string_value(info->buf, info->buf_size, false);
	jsonl_field_close();
	jsonl_struct_close();
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_ioctl_call(RNDZAPENTCNT, "FASTRPC_IOCTL_INIT_ATTACH",
			 "RNDZAPENTCNT", NULL);
	jsonl_ioctl_end();

	jsonl_ioctl_call(ARG_STR(RNDCLEARPOOL), NULL, NULL);
	jsonl_ioctl_end();

	jsonl_ioctl_call(ARG_STR(RNDRESEEDCRNG), NULL, NULL);
	jsonl_ioctl_end();

	jsonl_ioctl_call(_IO('R', 0xff), NULL, NULL, NULL);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_hex(0);
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
