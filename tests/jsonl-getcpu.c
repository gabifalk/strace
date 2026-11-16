/*
 * Check JSONL output of the getcpu syscall entry and exit events.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_getcpu

# include "jsonl.h"

static const char *side;

static void
jsonl_cpu(unsigned int index, const char *name, unsigned val)
{
	jsonl_arg_open(index, name, side);
	printf("\"type\": \"unsigned\", \"raw\": \"%u\", \"indirect\": true",
	       val);
	jsonl_arg_close(side);
}

int
main(int argc, char *argv[])
{
	TAIL_ALLOC_OBJECT_CONST_PTR(unsigned, cpu);
	TAIL_ALLOC_OBJECT_CONST_PTR(unsigned, node);

	long rc = syscall(__NR_getcpu, cpu, node, NULL);
	if (rc)
		perror_msg_and_skip("getcpu");

	side = (argc > 1 && !strcmp(argv[1], "merged")) ? "exit" : NULL;

	if (side) {
		jsonl_syscall_merged_open("getcpu", __NR_getcpu);
	} else {
		jsonl_syscall_open("getcpu", __NR_getcpu, true);
		printf("]}\n");
		jsonl_syscall_open("getcpu", __NR_getcpu, false);
	}

	jsonl_cpu(0, "cpu", *cpu);
	printf(", ");
	jsonl_cpu(1, "node", *node);
	printf(", ");
	jsonl_arg_open(2, "tcache", side);
	printf("\"type\": \"addr\", \"raw\": null");
	jsonl_arg_close(side);

	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_getcpu")

#endif
