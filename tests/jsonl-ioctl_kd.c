/*
 * Check JSONL output of char values: the diacr, base, and result fields
 * of struct kbdiacr in KDSKBDIACR ioctl command.
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

	TAIL_ALLOC_OBJECT_CONST_PTR(struct kbdiacrs, kbd);
	static const struct kbdiacr diacrs[] = {
		{ 'S', 'a', '\'' },
		{ '\t', '\\', 0x80 },
	};

	kbd->kb_cnt = ARRAY_SIZE(diacrs);
	memcpy(kbd->kbdiacr, diacrs, sizeof(diacrs));

	jsonl_ioctl_call(KDSKBDIACR, "KDSKBDIACR", NULL, kbd);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_struct_open();
	jsonl_field_unsigned("kb_cnt", true, kbd->kb_cnt);
	jsonl_field_open("kbdiacr", false);
	jsonl_array_open();
	for (size_t i = 0; i < ARRAY_SIZE(diacrs); ++i) {
		printf("%s{", i ? ", " : "");
		jsonl_struct_open();
		jsonl_field_char("diacr", true, diacrs[i].diacr);
		jsonl_field_char("base", false, diacrs[i].base);
		jsonl_field_char("result", false, diacrs[i].result);
		jsonl_struct_close();
		printf("}");
	}
	jsonl_array_close();
	jsonl_field_close();
	jsonl_struct_close();
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
