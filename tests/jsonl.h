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

# include <errno.h>
# include <inttypes.h>
# include <stdbool.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/stat.h>
# include <sys/sysmacros.h>

/*
 * Prefix of a jsonl-split "syscall" event of process pid
 * up to the "args" array bracket.
 */
static inline void
jsonl_syscall_open_pid(pid_t pid, const char *name, long scno, bool entering)
{
	printf("{\"event\": \"syscall\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, \"syscall\": "
	       "{\"type\": \"syscall\", \"name\": \"%s\", \"scno\": \"%ld\"}, "
	       "\"entering\": %s, \"args\": [",
	       (int) pid, name, scno, entering ? "true" : "false");
}

/* Prefix of a jsonl-split "syscall" event up to the "args" array bracket. */
static inline void
jsonl_syscall_open(const char *name, long scno, bool entering)
{
	jsonl_syscall_open_pid(getpid(), name, scno, entering);
}

/*
 * Prefix of a jsonl-merged "syscall" event of process pid
 * up to the "args" array bracket.
 */
static inline void
jsonl_syscall_merged_open_pid(pid_t pid, const char *name, long scno)
{
	printf("{\"event\": \"syscall\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, \"syscall\": "
	       "{\"type\": \"syscall\", \"name\": \"%s\", \"scno\": \"%ld\"}, "
	       "\"args\": [",
	       (int) pid, name, scno);
}

/* Prefix of a jsonl-merged "syscall" event up to the "args" array bracket. */
static inline void
jsonl_syscall_merged_open(const char *name, long scno)
{
	jsonl_syscall_merged_open_pid(getpid(), name, scno);
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

/* Emit len bytes of str as a decimal-integer array value. */
static inline void
jsonl_bytes_n(const char *str, size_t len)
{
	printf("[");
	for (size_t i = 0; i < len; ++i)
		printf("%s%u", i ? ", " : "", (unsigned char) str[i]);
	printf("]");
}

/* Emit a NUL-terminated string as a decimal-integer array value. */
static inline void
jsonl_bytes(const char *str)
{
	jsonl_bytes_n(str, strlen(str));
}

/* The ", \"truncated\": true" marker that closes a truncated value. */
static inline void
jsonl_truncated(void)
{
	printf(", \"truncated\": true");
}

/* The fields of an fd value, with a path fd_info when path is given. */
static inline void
jsonl_fd(int fd, const char *path, bool deleted)
{
	printf("\"type\": \"fd\", \"raw\": \"%d\"", fd);
	if (path) {
		printf(", \"fd_info\": {\"type\": \"path\", \"path\": ");
		jsonl_bytes(path);
		printf("%s}", deleted ? ", \"deleted\": true" : "");
	}
}

/* The fields of an fd value with the dev fd_info of a device file. */
static inline void
jsonl_fd_dev(int fd, const char *path, const struct stat *st)
{
	printf("\"type\": \"fd\", \"raw\": \"%d\","
	       " \"fd_info\": {\"type\": \"dev_t\", \"path\": ", fd);
	jsonl_bytes(path);
	printf(", \"kind\": \"%s\", \"major\": \"%u\", \"minor\": \"%u\"}",
	       S_ISBLK(st->st_mode) ? "block" : "char",
	       major(st->st_rdev), minor(st->st_rdev));
}

/*
 * The "return" field of a syscall event: an errno object for rc == -1,
 * otherwise a value of the given type.
 */
static inline void
jsonl_return(const char *type, long rc)
{
	if (rc == -1) {
		printf("\"return\": {\"type\": \"decimal\", \"raw\": \"-1\","
		       " \"errno\": {\"type\": \"errno\", \"raw\": \"%d\","
		       " \"sym\": \"%s\", \"strerror\": \"%s\"}}",
		       errno, errno2name(errno), strerror(errno));
	} else {
		printf("\"return\": {\"type\": \"%s\", \"raw\": \"%ld\"}",
		       type, rc);
	}
}

/* The "exit" event of the current process. */
static inline void
jsonl_exit_event(int status)
{
	printf("{\"event\": \"exit\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, \"status\": %d}\n",
	       (int) getpid(), status);
}

/* A raw xlat value string: "0" for zero, "0x..." otherwise. */
static inline void
jsonl_xlat_raw(uint64_t val)
{
	printf("\"%#" PRIx64 "\"", val);
}

/*
 * The fields of a const value: "type": "const", "raw": <raw>, and a "sym"
 * (when sym is non-NULL) or "table" (when sym is NULL and table is not).
 * The raw value is formatted as an xlat value (hex) when hex is set, and
 * as a decimal value otherwise.
 */
static inline void
jsonl_const(uint64_t val, bool hex, const char *sym, const char *table)
{
	printf("\"type\": \"const\", \"raw\": ");
	if (hex)
		jsonl_xlat_raw(val);
	else
		printf("\"%" PRIu64 "\"", val);
	if (sym)
		printf(", \"sym\": \"%s\"", sym);
	else if (table)
		printf(", \"table\": \"%s\"", table);
}

/*
 * The fields of a signal value: "type": "signal", "raw": "<nr>" (decimal),
 * and a "sym" when sym is non-NULL.
 */
static inline void
jsonl_signal(int nr, const char *sym)
{
	printf("\"type\": \"signal\", \"raw\": \"%d\"", nr);
	if (sym)
		printf(", \"sym\": \"%s\"", sym);
}

/*
 * The fields of a ticks value: "type": "ticks", "raw": "<raw>", with the
 * clock frequency "hz" and decimal "precision" derived from _SC_CLK_TCK
 * the same way print_clock_t does.
 */
static inline void
jsonl_ticks(uint64_t raw)
{
	const long hz = sysconf(_SC_CLK_TCK);
	unsigned int precision = 0;

	for (long t = hz - 1; t > 0 && precision < 9; t /= 10)
		++precision;

	printf("\"type\": \"ticks\", \"raw\": \"%" PRIu64 "\", "
	       "\"hz\": \"%ld\", \"precision\": \"%u\"", raw, hz, precision);
}

/*
 * Open a flags value with its single implicit group, up to the elems array:
 * "type": "flags", "raw": "<raw>", "groups": [{"raw": "<raw>",
 * "group_type": "flags", "elems": [
 */
static inline void
jsonl_flags_open(uint64_t raw)
{
	printf("\"type\": \"flags\", \"raw\": ");
	jsonl_xlat_raw(raw);
	printf(", \"groups\": [{\"raw\": ");
	jsonl_xlat_raw(raw);
	printf(", \"group_type\": \"flags\", \"elems\": [");
}

/* A matched flag: {"type": "const", "raw": "<raw>", "sym": "<sym>"}. */
static inline void
jsonl_flags_const(uint64_t raw, const char *sym, bool first)
{
	printf("%s{\"type\": \"const\", \"raw\": ", first ? "" : ", ");
	jsonl_xlat_raw(raw);
	printf(", \"sym\": \"%s\"}", sym);
}

/*
 * The leftover bits: {"type": "remainder", "raw": "<raw>"[, "sym": "<sym>"]}.
 * A NULL sym emits no "sym" field; an empty sym emits "sym": "".
 */
static inline void
jsonl_flags_remainder(uint64_t raw, const char *sym, bool first)
{
	printf("%s{\"type\": \"remainder\", \"raw\": ", first ? "" : ", ");
	jsonl_xlat_raw(raw);
	if (sym)
		printf(", \"sym\": \"%s\"", sym);
	printf("}");
}

/* Close the elems array, the group, and the groups array: ]}]. */
static inline void
jsonl_flags_close(void)
{
	printf("]}]");
}

#endif /* !STRACE_TESTS_JSONL_H */
