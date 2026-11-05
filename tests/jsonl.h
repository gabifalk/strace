/*
 * Helpers for emitting expected JSONL output in strace tests.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef STRACE_TESTS_JSONL_H
# define STRACE_TESTS_JSONL_H

# include <stdbool.h>
# include <stdio.h>
# include <unistd.h>

/* Prefix of a jsonl-split "syscall" event up to the "args" array bracket. */
static inline void
jsonl_syscall_open(const char *name, long scno, bool entering)
{
	printf("{\"event\": \"syscall\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, \"syscall\": "
	       "{\"type\": \"syscall\", \"name\": \"%s\", \"scno\": \"%ld\"}, "
	       "\"entering\": %s, \"args\": [",
	       (int) getpid(), name, scno, entering ? "true" : "false");
}

/* Prefix of a jsonl-merged "syscall" event up to the "args" array bracket. */
static inline void
jsonl_syscall_merged_open(const char *name, long scno)
{
	printf("{\"event\": \"syscall\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, \"syscall\": "
	       "{\"type\": \"syscall\", \"name\": \"%s\", \"scno\": \"%ld\"}, "
	       "\"args\": [",
	       (int) getpid(), name, scno);
}

/*
 * Prefix of an "args" element: its index and name meta-fields, and in
 * merged mode the opening brace of the value object of the given side.
 */
static inline void
jsonl_arg_open(unsigned int index, const char *name, const char *side)
{
	printf("{\"index\": %u, \"arg\": \"%s\", ", index, name);
	if (side)
		printf("\"%s\": {", side);
}

/* Closing braces of an "args" element opened by jsonl_arg_open. */
static inline void
jsonl_arg_close(const char *side)
{
	printf(side ? "}}" : "}");
}

#endif /* !STRACE_TESTS_JSONL_H */
