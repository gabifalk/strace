/*
 * Check JSONL output of the PERF_EVENT_IOC_MODIFY_ATTRIBUTES ioctl command:
 * the precise_ip field of struct perf_event_attr is a perf_precise_ip value,
 * and its __reserved_1 field is a reserved_bits value.
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

# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <linux/perf_event.h>

/* The bits of the bit field of offset off and width w in the flags word. */
static uint64_t
bit_field(uint64_t val, unsigned int off, unsigned int w)
{
# ifdef WORDS_BIGENDIAN
	return val << (64 - off - w);
# else
	return val << off;
# endif
}

int
main(int argc, char *argv[])
{
	static const char *const skids[] = {
		"arbitrary", "constant", "requested_zero", "zero"
	};
	static const unsigned int reserved = 0x2bad;

	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_ARR(uint64_t, attr, 8);
	memset(attr, 0, 8 * sizeof(*attr));
	const uint32_t size = 64;
	memcpy((char *) attr + 4, &size, sizeof(size));

	for (unsigned int ip = 0; ip < 4; ++ip) {
		attr[5] = bit_field(ip, 15, 2) | bit_field(reserved, 38, 26);

		jsonl_ioctl_call(PERF_EVENT_IOC_MODIFY_ATTRIBUTES,
				 "PERF_EVENT_IOC_MODIFY_ATTRIBUTES", NULL, attr);
		jsonl_ioctl_argp_open("argp", false);
		jsonl_struct_open();
		jsonl_field_const("type", true, 0, "PERF_TYPE_HARDWARE", NULL);
		jsonl_field_const("size", false, size, "PERF_ATTR_SIZE_VER0",
				  NULL);
		jsonl_field_const("config", false, 0,
				  "PERF_COUNT_HW_CPU_CYCLES", NULL);
		jsonl_field_unsigned("sample_period", false, 0);
		printf(", \"sample_type\": {");
		jsonl_flags_open(0);
		jsonl_flags_close();
		printf("}, \"read_format\": {");
		jsonl_flags_open(0);
		jsonl_flags_close();
		printf("}, \"precise_ip\": {\"type\": \"perf_precise_ip\","
		       " \"raw\": \"%u\", \"skid\": \"%s\"}", ip, skids[ip]);
		printf(", \"__reserved_1\": {\"type\": \"reserved_bits\","
		       " \"raw\": \"%#x\", \"first_bit\": \"38\","
		       " \"last_bit\": \"63\"}", reserved);
		jsonl_struct_close();
		jsonl_truncated();
		jsonl_ioctl_argp_close();
		jsonl_ioctl_end();
	}

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
