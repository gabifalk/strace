/*
 * Check JSONL output of the bpf syscall: split attr structures
 * and extra data beyond the known part of an attr structure.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_bpf

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <linux/bpf.h>
# include "bpf_attr.h"

static bool merged;

static void
syscall_open(bool entering)
{
	if (merged)
		jsonl_syscall_merged_open("bpf", __NR_bpf);
	else
		jsonl_syscall_open("bpf", __NR_bpf, entering);
}

static void
cmd_arg(unsigned int cmd, const char *sym)
{
	jsonl_arg_open(0, "cmd", merged ? "entry" : NULL);
	jsonl_const(cmd, true, sym, NULL);
	jsonl_arg_close(merged ? "entry" : NULL);
}

static void
size_arg(unsigned int size, const char *side)
{
	printf(", ");
	jsonl_arg_open(2, "size", side);
	jsonl_unsigned(size);
	jsonl_arg_close(side);
}

static void
syscall_close(long rc, int saved_errno)
{
	printf("], ");
	errno = saved_errno;
	jsonl_return_open("unsigned", rc);
	printf("}}\n");
}

static void
test_get_next_id(void)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(struct BPF_PROG_GET_NEXT_ID_struct, attr);
	attr->start_id = 0xfacefeed;
	attr->next_id = 0;

	long rc = syscall(__NR_bpf, BPF_PROG_GET_NEXT_ID, attr, sizeof(*attr));
	int saved_errno = errno;

	syscall_open(true);
	cmd_arg(BPF_PROG_GET_NEXT_ID, "BPF_PROG_GET_NEXT_ID");
	printf(", ");
	jsonl_arg_open(1, "attr", merged ? "entry" : NULL);
	jsonl_struct_open();
	jsonl_field_unsigned("start_id", true, 0xfacefeed);
	jsonl_struct_close();
	printf(", \"split\": true");
	if (merged) {
		printf("}, \"exit\": {");
	} else {
		printf("}]}\n");
		syscall_open(false);
		jsonl_arg_open(1, "attr", NULL);
	}
	jsonl_struct_open();
	jsonl_field_unsigned("next_id", true, attr->next_id);
	jsonl_struct_close();
	printf(", \"split\": true}");
	if (merged)
		printf("}");
	size_arg(sizeof(*attr), merged ? "exit" : NULL);
	syscall_close(rc, saved_errno);
}

static void
test_extra_data(void)
{
	static const char extra[] = { 0, 0x31, 0, 0x32 };
	struct {
		struct BPF_MAP_FREEZE_struct attr;
		char extra[sizeof(extra)];
	} *buf;
	TAIL_ALLOC_OBJECT_CONST_PTR(typeof(*buf), p);
	buf = p;
	buf->attr.map_fd = -1;
	memcpy(buf->extra, extra, sizeof(extra));

	long rc = syscall(__NR_bpf, BPF_MAP_FREEZE, buf, sizeof(*buf));
	int saved_errno = errno;

	syscall_open(true);
	cmd_arg(BPF_MAP_FREEZE, "BPF_MAP_FREEZE");
	printf(", ");
	jsonl_arg_open(1, "attr", merged ? "entry" : NULL);
	jsonl_struct_open();
	jsonl_field_open("map_fd", true);
	jsonl_fd(-1, NULL, false);
	jsonl_field_close();
	printf("}, \"extra_data\": {\"type\": \"bytes\", \"offset\": \"%zu\","
	       " \"size\": \"%zu\", \"value\": ",
	       sizeof(buf->attr), sizeof(extra));
	jsonl_bytes_n(extra, sizeof(extra));
	printf("}");
	jsonl_arg_close(merged ? "entry" : NULL);
	size_arg(sizeof(*buf), merged ? "entry" : NULL);
	if (!merged) {
		printf("]}\n");
		syscall_open(false);
	}
	syscall_close(rc, saved_errno);
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	test_get_next_id();
	test_extra_data();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_bpf")

#endif
