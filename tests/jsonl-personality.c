/*
 * Check JSONL output of the persona returned by personality as a flags
 * value whose groups are the type and the flags.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_personality

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/personality.h>

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "entry" : NULL;

	const long rc = syscall(__NR_personality, 0xffffffff);
	if (rc != PER_LINUX)
		error_msg_and_skip("personality: unexpected persona %#lx", rc);

	if (merged)
		jsonl_syscall_merged_open("personality", __NR_personality);
	else
		jsonl_syscall_open("personality", __NR_personality, true);
	jsonl_arg_open(0, "persona", side);
	jsonl_hex(0xffffffff);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("personality", __NR_personality, false);
	}
	printf("], \"return\": {");
	jsonl_flags_open(PER_LINUX);
	jsonl_flags_const(PER_LINUX, "PER_LINUX", true);
	jsonl_flags_close();
	printf("}}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_personality")

#endif
