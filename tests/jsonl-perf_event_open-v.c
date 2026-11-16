/*
 * Check JSONL output of perf_event_open attr printed in full (-v): the
 * non-zero __reserved_2 field is a reserved_bytes value.
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

# ifndef PERF_ATTR_SIZE_VER5
#  define PERF_ATTR_SIZE_VER5 112
# endif

static const char *const flags1[] = {
	"disabled", "inherit", "pinned", "exclusive", "exclude_user",
	"exclude_kernel", "exclude_hv", "exclude_idle", "mmap", "comm",
	"freq", "inherit_stat", "enable_on_exec", "task", "watermark",
};

static const char *const flags2[] = {
	"mmap_data", "sample_id_all", "exclude_host", "exclude_guest",
	"exclude_callchain_kernel", "exclude_callchain_user", "mmap2",
	"comm_exec", "use_clockid", "context_switch", "write_backward",
	"namespaces", "ksymbol", "bpf_event", "aux_output", "cgroup",
	"text_poke", "build_id", "inherit_thread", "remove_on_exec",
	"sigtrap",
};

static void
size_field(void)
{
	jsonl_field_const("size", true, PERF_ATTR_SIZE_VER5,
			  "PERF_ATTR_SIZE_VER5", NULL);
}

static void
empty_flags_field(const char *name)
{
	jsonl_field_open(name, false);
	jsonl_flags_open(0);
	jsonl_flags_close();
	jsonl_field_close();
}

static void
zero_fields(const char *const *names, size_t n)
{
	for (size_t i = 0; i < n; ++i)
		jsonl_field_unsigned(names[i], false, 0);
}

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const exit_side = merged ? "exit" : NULL;
	static const uint16_t reserved_2 = 0xefee;

	TAIL_ALLOC_OBJECT_CONST_PTR(struct perf_event_attr, attr);
	memset(attr, 0, sizeof(*attr));
	attr->type = PERF_TYPE_SOFTWARE;
	attr->size = PERF_ATTR_SIZE_VER5;
	attr->config = PERF_COUNT_SW_CPU_CLOCK;
	memcpy((char *) attr + 110, &reserved_2, sizeof(reserved_2));

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
	jsonl_field_const("type", true, PERF_TYPE_SOFTWARE,
			  "PERF_TYPE_SOFTWARE", NULL);
	printf(", ");
	size_field();
	jsonl_field_const("config", false, PERF_COUNT_SW_CPU_CLOCK,
			  "PERF_COUNT_SW_CPU_CLOCK", NULL);
	jsonl_field_unsigned("sample_period", false, 0);
	empty_flags_field("sample_type");
	empty_flags_field("read_format");
	zero_fields(flags1, ARRAY_SIZE(flags1));
	printf(", \"precise_ip\": {\"type\": \"perf_precise_ip\","
	       " \"raw\": \"0\", \"skid\": \"arbitrary\"}");
	zero_fields(flags2, ARRAY_SIZE(flags2));
	jsonl_field_unsigned("wakeup_events", false, 0);
	jsonl_field_hex("config1", false, 0);
	jsonl_field_hex("config2", false, 0);
	jsonl_field_hex("sample_regs_user", false, 0);
	jsonl_field_hex("sample_regs_intr", false, 0);
	jsonl_field_unsigned("aux_watermark", false, 0);
	jsonl_field_unsigned("sample_max_stack", false, 0);
	printf(", \"__reserved_2\": {\"type\": \"reserved_bytes\","
	       " \"raw\": \"%#x\", \"first_byte\": \"110\","
	       " \"last_byte\": \"111\", \"reserved_inline\": true}",
	       reserved_2);
	jsonl_struct_close();
	printf(", \"split\": true");
	jsonl_arg_close(exit_side);

	printf(", ");
	jsonl_arg_open(1, "pid", exit_side);
	jsonl_decimal(-1);
	jsonl_arg_close(exit_side);
	printf(", ");
	jsonl_arg_open(2, "cpu", exit_side);
	jsonl_decimal(-1);
	jsonl_arg_close(exit_side);
	printf(", ");
	jsonl_arg_open(3, "group_fd", exit_side);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(exit_side);
	printf(", ");
	jsonl_arg_open(4, "flags", exit_side);
	jsonl_flags_open(0);
	jsonl_flags_close();
	jsonl_arg_close(exit_side);
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
