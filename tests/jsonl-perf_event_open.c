/*
 * Check JSONL output of the perf_event_open syscall: the split attr
 * structure and the flags groups of a PERF_TYPE_HW_CACHE event config.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_perf_event_open

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <linux/perf_event.h>

static bool merged;

static void
size_field(void)
{
	jsonl_field_const("size", true, PERF_ATTR_SIZE_VER0,
			  "PERF_ATTR_SIZE_VER0", NULL);
}

static void
shift_group(uint64_t raw, const char *value, const char *shift, bool first)
{
	printf("%s{\"raw\": ", first ? "" : ", ");
	jsonl_xlat_raw(raw);
	printf(", \"group_type\": \"shift\", \"elems\": [{\"type\": \"shift\","
	       " \"raw\": ");
	jsonl_xlat_raw(raw);
	printf(", \"value\": {%s}, \"shift\": \"%s\"}]}", value, shift);
}

static void
empty_flags(void)
{
	jsonl_flags_open(0);
	jsonl_flags_close();
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_PTR(struct perf_event_attr, attr);
	memset(attr, 0, sizeof(*attr));
	attr->type = PERF_TYPE_HW_CACHE;
	attr->size = PERF_ATTR_SIZE_VER0;
	attr->config = 0x2a00000000ULL | PERF_COUNT_HW_CACHE_RESULT_MISS << 16
		       | PERF_COUNT_HW_CACHE_OP_PREFETCH << 8
		       | PERF_COUNT_HW_CACHE_DTLB;

	long rc = syscall(__NR_perf_event_open, attr, -1, -1, -1, 0);
	int saved_errno = errno;

	if (merged) {
		jsonl_syscall_merged_open("perf_event_open",
					  __NR_perf_event_open);
		jsonl_arg_open(0, "attr", "entry");
	} else {
		jsonl_syscall_open("perf_event_open", __NR_perf_event_open,
				   true);
		jsonl_arg_open(0, "attr", NULL);
	}
	jsonl_struct_open();
	size_field();
	jsonl_struct_close();
	printf(", \"split\": true");
	if (merged) {
		printf("}, \"exit\": {");
	} else {
		printf("}]}\n");
		jsonl_syscall_open("perf_event_open", __NR_perf_event_open,
				   false);
		jsonl_arg_open(0, "attr", NULL);
	}
	jsonl_struct_open();
	jsonl_field_const("type", true, PERF_TYPE_HW_CACHE,
			  "PERF_TYPE_HW_CACHE", NULL);
	printf(", ");
	size_field();
	jsonl_field_open("config", false);
	jsonl_flags_groups_open(attr->config);
	shift_group(0x2a00000000ULL, "\"type\": \"hex\", \"raw\": \"0x2a\"",
		    "32", true);
	shift_group(PERF_COUNT_HW_CACHE_RESULT_MISS << 16,
		    "\"type\": \"const\", \"raw\": \"0x1\","
		    " \"sym\": \"PERF_COUNT_HW_CACHE_RESULT_MISS\"", "16",
		    false);
	shift_group(PERF_COUNT_HW_CACHE_OP_PREFETCH << 8,
		    "\"type\": \"const\", \"raw\": \"0x2\","
		    " \"sym\": \"PERF_COUNT_HW_CACHE_OP_PREFETCH\"", "8",
		    false);
	jsonl_flags_group_open(PERF_COUNT_HW_CACHE_DTLB, false);
	jsonl_flags_const(PERF_COUNT_HW_CACHE_DTLB, "PERF_COUNT_HW_CACHE_DTLB",
			  true);
	jsonl_flags_group_close();
	jsonl_flags_groups_close();
	jsonl_field_close();
	jsonl_field_unsigned("sample_period", false, 0);
	jsonl_field_open("sample_type", false);
	empty_flags();
	jsonl_field_close();
	jsonl_field_open("read_format", false);
	empty_flags();
	jsonl_field_close();
	printf(", \"precise_ip\": {\"type\": \"perf_precise_ip\","
	       " \"raw\": \"0\", \"skid\": \"arbitrary\"}");
	jsonl_struct_close();
	jsonl_truncated();
	printf(", \"split\": true");
	jsonl_arg_close(merged ? "exit" : NULL);

	printf(", ");
	jsonl_arg_open(1, "pid", merged ? "exit" : NULL);
	jsonl_decimal(-1);
	jsonl_arg_close(merged ? "exit" : NULL);
	printf(", ");
	jsonl_arg_open(2, "cpu", merged ? "exit" : NULL);
	jsonl_decimal(-1);
	jsonl_arg_close(merged ? "exit" : NULL);
	printf(", ");
	jsonl_arg_open(3, "group_fd", merged ? "exit" : NULL);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(merged ? "exit" : NULL);
	printf(", ");
	jsonl_arg_open(4, "flags", merged ? "exit" : NULL);
	empty_flags();
	jsonl_arg_close(merged ? "exit" : NULL);
	printf("], ");
	errno = saved_errno;
	jsonl_return("fd", rc);
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_perf_event_open")

#endif
