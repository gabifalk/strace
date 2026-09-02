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

/*
 * Prefix of one JSONL "syscall" event in jsonl-split mode, up to and
 * including the opening bracket of the "args" array.  The caller prints
 * the array elements, a closing "]", and, on exit, the "return" object.
 */
static inline void
jsonl_syscall_open(const char *name, long scno, bool entering)
{
	printf("{\"event\": \"syscall\", \"pid\": %d, \"syscall\": "
	       "{\"type\": \"syscall\", \"name\": \"%s\", \"scno\": \"%ld\"}, "
	       "\"entering\": %s, \"args\": [",
	       (int) getpid(), name, scno, entering ? "true" : "false");
}

#endif /* !STRACE_TESTS_JSONL_H */
