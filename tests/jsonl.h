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

# if defined MPERS_IS_m32
#  define JSONL_PERSONALITY 1
# elif defined MPERS_IS_mx32
#  define JSONL_PERSONALITY 2
# else
#  define JSONL_PERSONALITY 0
# endif

/* Prefix of a jsonl-split "syscall" event up to the "args" array bracket. */
static inline void
jsonl_syscall_open(const char *name, long scno, bool entering)
{
	printf("{\"event\": \"syscall\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, \"syscall\": "
	       "{\"type\": \"syscall\", \"name\": \"%s\", \"scno\": \"%ld\", "
	       "\"personality\": %d}, "
	       "\"entering\": %s, \"args\": [",
	       (int) getpid(), name, scno, JSONL_PERSONALITY,
	       entering ? "true" : "false");
}

/* Prefix of an "args" element: its index and name meta-fields. */
static inline void
jsonl_arg_open(unsigned int index, const char *name)
{
	printf("{\"index\": %u, \"arg\": \"%s\", ", index, name);
}

/* The "exit" event of the current process. */
static inline void
jsonl_exit_event(int status)
{
	printf("{\"event\": \"exit\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, \"status\": %d}\n",
	       (int) getpid(), status);
}

#endif /* !STRACE_TESTS_JSONL_H */
