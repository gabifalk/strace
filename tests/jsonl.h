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
# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/stat.h>
# include <sys/sysmacros.h>

# if defined MPERS_IS_m32
#  define JSONL_PERSONALITY 1
# elif defined MPERS_IS_mx32
#  define JSONL_PERSONALITY 2
# else
#  define JSONL_PERSONALITY 0
# endif

/*
 * Prefix of a jsonl-split "syscall" event of process pid
 * up to the "args" array bracket.
 */
static inline void
jsonl_syscall_open_pid(pid_t pid, const char *name, long scno, bool entering)
{
	printf("{\"event\": \"syscall\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, \"syscall\": "
	       "{\"type\": \"syscall\", \"name\": \"%s\", \"scno\": \"%ld\", "
	       "\"personality\": %d}, "
	       "\"entering\": %s, \"args\": [",
	       (int) pid, name, scno, JSONL_PERSONALITY,
	       entering ? "true" : "false");
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
	       "{\"type\": \"syscall\", \"name\": \"%s\", \"scno\": \"%ld\", "
	       "\"personality\": %d}, "
	       "\"args\": [",
	       (int) pid, name, scno, JSONL_PERSONALITY);
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
 * The fields of a standalone device-number value: "type": "dev_t",
 * the packed "raw" number (hex), and its "major" and "minor" components.
 */
static inline void
jsonl_dev_t(unsigned long long dev)
{
	printf("\"type\": \"dev_t\", \"raw\": \"%#llx\", "
	       "\"major\": \"%u\", \"minor\": \"%u\"",
	       dev, major(dev), minor(dev));
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
 * (when sym is non-NULL) or "sym_pattern" (when sym is NULL and sym_pattern
 * is not).  The raw value is formatted as an xlat value (hex) when hex is
 * set, and as a decimal value otherwise.
 */
static inline void
jsonl_const(uint64_t val, bool hex, const char *sym, const char *sym_pattern)
{
	printf("\"type\": \"const\", \"raw\": ");
	if (hex)
		jsonl_xlat_raw(val);
	else
		printf("\"%" PRIu64 "\"", val);
	if (sym)
		printf(", \"sym\": \"%s\"", sym);
	else if (sym_pattern)
		printf(", \"sym_pattern\": \"%s\"", sym_pattern);
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

/* The fields of a time_t value: "type": "time_t", "raw": "<sec>" (decimal). */
static inline void
jsonl_time_t(long long sec)
{
	printf("\"type\": \"time_t\", \"raw\": \"%lld\"", sec);
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

/* The fields of an addr value: a hex "raw", or null for a NULL pointer. */
static inline void
jsonl_addr(const void *p)
{
	if (p)
		printf("\"type\": \"addr\", \"raw\": \"%#lx\"",
		       (unsigned long) (uintptr_t) p);
	else
		printf("\"type\": \"addr\", \"raw\": null");
}

/* The fields of an unsigned value. */
static inline void
jsonl_unsigned(uint64_t val)
{
	printf("\"type\": \"unsigned\", \"raw\": \"%" PRIu64 "\"", val);
}

/* The fields of a decimal value. */
static inline void
jsonl_decimal(int64_t val)
{
	printf("\"type\": \"decimal\", \"raw\": \"%" PRId64 "\"", val);
}

/* The fields of a hex value. */
static inline void
jsonl_hex(uint64_t val)
{
	printf("\"type\": \"hex\", \"raw\": ");
	jsonl_xlat_raw(val);
}

/* The fields of a bytes value of len bytes, optionally marked truncated. */
static inline void
jsonl_bytes_value(const void *p, size_t len, bool truncated)
{
	printf("\"type\": \"bytes\", \"value\": ");
	jsonl_bytes_n(p, len);
	if (truncated)
		jsonl_truncated();
}

/* The fields of a string value of len bytes, optionally marked truncated. */
static inline void
jsonl_string_value(const void *p, size_t len, bool truncated)
{
	printf("\"type\": \"string\", \"value\": ");
	jsonl_bytes_n(p, len);
	if (truncated)
		jsonl_truncated();
}

/* The fields of a path value of a NUL-terminated path. */
static inline void
jsonl_path_value(const char *path)
{
	printf("\"type\": \"path\", \"value\": ");
	jsonl_bytes(path);
}

/* The fields of an ifindex value, with an "ifname" when ifname is non-NULL. */
static inline void
jsonl_ifindex(unsigned int index, const char *ifname)
{
	printf("\"type\": \"ifindex\", \"raw\": \"%u\"", index);
	if (ifname)
		printf(", \"ifname\": \"%s\"", ifname);
}

/* The fields of a mac value: len colon-separated hex octets. */
static inline void
jsonl_mac(const void *addr, size_t len)
{
	printf("\"type\": \"mac\", \"value\": \"");
	for (size_t i = 0; i < len; ++i)
		printf("%s%02x", i ? ":" : "",
		       ((const unsigned char *) addr)[i]);
	printf("\"");
}

/* The fields of an ip_addr value: the family const and the address text. */
static inline void
jsonl_ip_addr(int af, const char *af_sym, const char *addr)
{
	printf("\"type\": \"ip_addr\", \"family\": {");
	jsonl_const(af, true, af_sym, NULL);
	printf("}, \"addr\": \"%s\"", addr);
}

/* The suffix of a value of size bytes stored in network byte order. */
static inline void
jsonl_byteorder_big(size_t size)
{
	printf(", \"byteorder\": \"big\", \"size\": %zu", size);
}

/* Open a struct value up to its first field: "type": "struct", "fields": { */
static inline void
jsonl_struct_open(void)
{
	printf("\"type\": \"struct\", \"fields\": {");
}

/* Close the fields object of a struct value. */
static inline void
jsonl_struct_close(void)
{
	printf("}");
}

/*
 * Close the fields object of a struct value whose next field at addr
 * could not be fetched, and add the address.
 */
static inline void
jsonl_struct_close_fetch_failed(const void *addr)
{
	printf("}, \"fetch_failed_addr\": \"%#lx\"",
	       (unsigned long) (uintptr_t) addr);
}

/* Open a struct field up to the fields of its value: [, ]"<name>": { */
static inline void
jsonl_field_open(const char *name, bool first)
{
	printf("%s\"%s\": {", first ? "" : ", ", name);
}

/* Close a struct field opened by jsonl_field_open. */
static inline void
jsonl_field_close(void)
{
	printf("}");
}

/* Open an array value up to its first element: "type": "array", "elems": [ */
static inline void
jsonl_array_open(void)
{
	printf("\"type\": \"array\", \"elems\": [");
}

/* Close the elems array of an array value. */
static inline void
jsonl_array_close(void)
{
	printf("]");
}

/*
 * The tail of an array whose next element at addr could not be fetched:
 * the closing bracket and the address.
 */
static inline void
jsonl_array_fetch_failed(const void *addr)
{
	printf("], \"fetch_failed_addr\": \"%#lx\"",
	       (unsigned long) (uintptr_t) addr);
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

/*
 * The fields of a flags value: the entries of the known table that are set
 * in raw (a zero entry matches a zero raw only), in the order of the table,
 * followed by a remainder with the leftover bits; dflt is the "sym" of the
 * remainder when no entry has matched.
 */
static inline void
jsonl_flags(uint64_t raw, const struct strval32 *known, size_t n,
	    const char *dflt)
{
	uint64_t rem = raw;
	bool first = true;

	jsonl_flags_open(raw);
	for (size_t i = 0; i < n; ++i) {
		const uint64_t val = known[i].val;

		if (val ? (raw & val) != val : raw != 0)
			continue;
		jsonl_flags_const(val, known[i].str, first);
		rem &= ~val;
		first = false;
	}
	if (rem)
		jsonl_flags_remainder(rem, first ? dflt : NULL, first);
	jsonl_flags_close();
}

#endif /* !STRACE_TESTS_JSONL_H */
