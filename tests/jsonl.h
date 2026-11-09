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

/* Prefix of an "args" element: its index and name meta-fields. */
static inline void
jsonl_arg_open(unsigned int index, const char *name)
{
	printf("{\"index\": %u, \"arg\": \"%s\", ", index, name);
}

#endif /* !STRACE_TESTS_JSONL_H */
