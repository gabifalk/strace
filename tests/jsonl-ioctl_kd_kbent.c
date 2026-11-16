/*
 * Check JSONL output of kd_key values: the kb_value field
 * of struct kbentry in KDSKBENT ioctl command.
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
# include <linux/keyboard.h>

static void
test_kbent_open(struct kbentry *kbe, unsigned short value)
{
	kbe->kb_table = K_NORMTAB;
	kbe->kb_index = 42;
	kbe->kb_value = value;

	jsonl_ioctl_call(KDSKBENT, "KDSKBENT", NULL, kbe);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_struct_open();
	jsonl_field_const("kb_table", true, K_NORMTAB, "K_NORMTAB", NULL);
	jsonl_field_unsigned("kb_index", false, 42);
	jsonl_field_open("kb_value", false);
	printf("\"type\": \"kd_key\", \"raw\": \"%#x\"", value);
}

static void
test_kbent_close(void)
{
	jsonl_field_close();
	jsonl_struct_close();
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();
}

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_PTR(struct kbentry, kbe);

	test_kbent_open(kbe, K_F2);
	printf(", \"sym\": \"K_F2\"");
	test_kbent_close();

	test_kbent_open(kbe, K(KT_LATIN, 'a'));
	printf(", \"key_type\": {");
	jsonl_const(KT_LATIN, true, "KT_LATIN", NULL);
	printf("}, \"key_value\": {");
	jsonl_char('a');
	printf("}");
	test_kbent_close();

	test_kbent_open(kbe, K(KT_SPEC, 0x80));
	printf(", \"key_type\": {");
	jsonl_const(KT_SPEC, true, "KT_SPEC", NULL);
	printf("}, \"key_value\": {");
	jsonl_hex(0x80);
	printf("}");
	test_kbent_close();

	test_kbent_open(kbe, K(0xf0, 0xd));
	printf(", \"key_type\": {");
	jsonl_const(0xf0, true, NULL, NULL);
	printf("}, \"key_value\": {");
	jsonl_hex(0xd);
	printf("}");
	test_kbent_close();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
