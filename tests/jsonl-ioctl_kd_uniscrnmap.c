/*
 * Check JSONL output of the elements of the Unicode screen map
 * in PIO_UNISCRNMAP ioctl command, including the elements in the
 * direct-to-font zone named UNI_DIRECT_BASE+offset.
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

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_ARR(unsigned short, map, E_TABSZ);
	memset(map, 0, E_TABSZ * sizeof(*map));
	map[0] = UNI_DIRECT_BASE + 0x41;
	map[1] = 0x41;
	map[2] = UNI_DIRECT_BASE;

	jsonl_ioctl_call(PIO_UNISCRNMAP, "PIO_UNISCRNMAP", NULL, map);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_array_open();
	printf("{");
	jsonl_const(map[0], true, "UNI_DIRECT_BASE+0x41", NULL);
	printf("}, {");
	jsonl_hex(map[1]);
	printf("}, {");
	jsonl_const(map[2], true, "UNI_DIRECT_BASE+0", NULL);
	printf("}");
	jsonl_array_close();
	jsonl_truncated();
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
