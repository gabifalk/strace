/*
 * Check JSONL output of the io_uring_register syscall: restriction
 * opcodes and the query list of IORING_REGISTER_QUERY.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_io_uring_register

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <linux/io_uring.h>
# include <linux/io_uring/query.h>

static bool merged;

static void
call_open(unsigned int opcode, const char *opcode_sym)
{
	const char *side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("io_uring_register",
					  __NR_io_uring_register);
	else
		jsonl_syscall_open("io_uring_register",
				   __NR_io_uring_register, true);
	jsonl_arg_open(0, "fd", side);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "opcode", side);
	jsonl_const(opcode, true, opcode_sym, NULL);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "arg", side);
}

static void
call_close(unsigned int nargs, long rc, int saved_errno)
{
	const char *side = merged ? "entry" : NULL;

	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(3, "nr_args", side);
	jsonl_unsigned(nargs);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("io_uring_register",
				   __NR_io_uring_register, false);
	}
	printf("], ");
	errno = saved_errno;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

static void
test_restrictions(void)
{
	TAIL_ALLOC_OBJECT_CONST_ARR(struct io_uring_restriction, r, 2);
	memset(r, 0, 2 * sizeof(*r));
	r[0].opcode = IORING_RESTRICTION_REGISTER_OP;
	r[0].register_op = IORING_REGISTER_BUFFERS;
	r[1].opcode = 0x89;
	r[1].register_op = 0xef;

	long rc = syscall(__NR_io_uring_register, -1,
			  IORING_REGISTER_RESTRICTIONS, r, 2);
	int saved_errno = errno;

	call_open(IORING_REGISTER_RESTRICTIONS,
		  "IORING_REGISTER_RESTRICTIONS");
	jsonl_array_open();
	printf("{");
	jsonl_struct_open();
	printf("\"opcode\": {\"type\": \"uring_restriction_op\","
	       " \"raw\": \"0\", \"sym\": \"IORING_RESTRICTION_REGISTER_OP\","
	       " \"fields\": {\"register_op\": {");
	jsonl_const(IORING_REGISTER_BUFFERS, true, "IORING_REGISTER_BUFFERS",
		    NULL);
	printf("}}}");
	jsonl_struct_close();
	printf("}, {");
	jsonl_struct_open();
	printf("\"opcode\": {\"type\": \"uring_restriction_op\","
	       " \"raw\": \"0x89\","
	       " \"sym_pattern\": \"IORING_RESTRICTION_???\","
	       " \"op\": \"0xef\"}");
	jsonl_struct_close();
	printf("}");
	jsonl_array_close();
	call_close(2, rc, saved_errno);
}

static void
query_entry(const struct io_uring_query_hdr *hdr)
{
	printf("{");
	jsonl_struct_open();
	jsonl_field_open("query_data", true);
	jsonl_addr(NULL);
	jsonl_field_close();
	jsonl_field_const("query_op", false, hdr->query_op,
			  "IO_URING_QUERY_SCQ", NULL);
	jsonl_field_unsigned("size", false, hdr->size);
	jsonl_field_decimal("result", false, hdr->result);
	jsonl_struct_close();
	printf("}");
}

static void
test_query(void)
{
	TAIL_ALLOC_OBJECT_CONST_ARR(struct io_uring_query_hdr, hdr, 2);
	memset(hdr, 0, 2 * sizeof(*hdr));
	hdr[0].next_entry = (uintptr_t) &hdr[1];
	hdr[0].query_op = IO_URING_QUERY_SCQ;
	hdr[0].size = 16;
	hdr[0].result = -1;
	hdr[1].query_op = IO_URING_QUERY_SCQ;
	hdr[1].result = 42;

	long rc = syscall(__NR_io_uring_register, -1, IORING_REGISTER_QUERY,
			  hdr, 0);
	int saved_errno = errno;

	call_open(IORING_REGISTER_QUERY, "IORING_REGISTER_QUERY");
	printf("\"type\": \"query_list\", \"elems\": [");
	query_entry(&hdr[0]);
	printf(", ");
	query_entry(&hdr[1]);
	printf("]");
	call_close(0, rc, saved_errno);
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	test_restrictions();
	test_query();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_io_uring_register")

#endif
