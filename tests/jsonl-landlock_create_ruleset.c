/*
 * Check JSONL output of a truncated struct argument.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_landlock_create_ruleset

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>

/* The attr struct with all known fields zeroed and a truncation marker. */
static void
attr_struct(void)
{
	static const char *const names[] = {
		"handled_access_fs", "handled_access_net", "scoped",
		"quiet_access_fs", "quiet_access_net", "quiet_scoped",
	};

	printf("\"type\": \"struct\", \"fields\": {");
	for (unsigned int i = 0; i < ARRAY_SIZE(names); ++i) {
		printf("%s\"%s\": {", i ? ", " : "", names[i]);
		jsonl_flags_open(0);
		jsonl_flags_close();
		printf("}");
	}
	jsonl_truncated();
	printf("}");
}

int
main(int argc, char *argv[])
{
	bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *side = merged ? "entry" : NULL;

	/* A size past the known struct makes the decoding truncated. */
	const unsigned long size = 64;
	char *const attr = tail_alloc(size);
	memset(attr, 0, size);

	long rc = syscall(__NR_landlock_create_ruleset, attr, size, 0UL);
	int err = errno;

	if (merged)
		jsonl_syscall_merged_open("landlock_create_ruleset",
					  __NR_landlock_create_ruleset);
	else
		jsonl_syscall_open("landlock_create_ruleset",
				   __NR_landlock_create_ruleset, true);
	jsonl_arg_open(0, "attr", side);
	attr_struct();
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "size", side);
	printf("\"type\": \"unsigned\", \"raw\": \"%lu\"", size);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "flags", side);
	jsonl_flags_open(0);
	jsonl_flags_close();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("landlock_create_ruleset",
				   __NR_landlock_create_ruleset, false);
	}
	printf("], ");
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_landlock_create_ruleset")

#endif
