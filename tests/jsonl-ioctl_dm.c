/*
 * Check JSONL output of device mapper ioctl commands whose decoding
 * stops partway: the decode_error of a struct dm_ioctl whose data_size
 * is too small, and of an unavailable struct dm_target_spec.
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
# include <stddef.h>
# include <string.h>
# include <linux/dm-ioctl.h>

struct dm_table_load {
	struct dm_ioctl ioc;
	char data[64];
};

static void
emit_version(const struct dm_ioctl *const ioc)
{
	printf("\"version\": {");
	jsonl_array_open();
	for (unsigned int i = 0; i < 3; ++i) {
		printf("%s{", i ? ", " : "");
		jsonl_unsigned(ioc->version[i]);
		printf("}");
	}
	jsonl_array_close();
	printf("}");
}

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_PTR(struct dm_table_load, s);
	memset(s, 0, sizeof(*s));

	s->ioc.version[0] = DM_VERSION_MAJOR;
	s->ioc.data_size = offsetof(struct dm_ioctl, data) - 1;
	jsonl_ioctl_call(DM_VERSION, "DM_VERSION", NULL, s);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_array_open();
	printf("{");
	jsonl_struct_open();
	emit_version(&s->ioc);
	jsonl_field_unsigned("data_size", false, s->ioc.data_size);
	jsonl_struct_close();
	printf(", \"decode_error\": \"data_size too small\"}");
	jsonl_array_close();
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	s->ioc.version[1] = 1;
	s->ioc.data_size = sizeof(*s);
	s->ioc.data_start = sizeof(*s) - 4;
	s->ioc.target_count = 1;
	jsonl_ioctl_call(DM_TABLE_LOAD, "DM_TABLE_LOAD", NULL, s);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_array_open();
	printf("{");
	jsonl_struct_open();
	emit_version(&s->ioc);
	jsonl_field_unsigned("data_size", false, s->ioc.data_size);
	jsonl_field_unsigned("data_start", false, s->ioc.data_start);
	jsonl_field_unsigned("target_count", false, s->ioc.target_count);
	printf(", \"flags\": {");
	jsonl_flags_open(0);
	jsonl_flags_close();
	printf("}");
	jsonl_struct_close();
	printf("}, {\"type\": \"unavailable\","
	       " \"decode_error\": \"misplaced struct dm_target_spec\"}");
	jsonl_array_close();
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
