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
# include "xlat.h"

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
 * The "return" field of a syscall event up to its closing brace: an errno
 * object for rc == -1, otherwise a value of the given type.
 */
static inline void
jsonl_return_open(const char *type, long rc)
{
	if (rc == -1) {
		printf("\"return\": {\"type\": \"decimal\", \"raw\": \"-1\","
		       " \"errno\": {\"type\": \"errno\", \"raw\": \"%d\","
		       " \"sym\": \"%s\", \"strerror\": \"%s\"}",
		       errno, errno2name(errno), strerror(errno));
	} else {
		printf("\"return\": {\"type\": \"%s\", \"raw\": \"%ld\"",
		       type, rc);
	}
}

/* The "return" field of a syscall event. */
static inline void
jsonl_return(const char *type, long rc)
{
	jsonl_return_open(type, rc);
	printf("}");
}

/* The "return" field of a syscall event whose return value is injected. */
static inline void
jsonl_return_injected(const char *type, long rc)
{
	jsonl_return_open(type, rc);
	printf(", \"injected\": true}");
}

/* The ", \"indirect\": true" marker of a value fetched through a pointer. */
static inline void
jsonl_indirect(void)
{
	printf(", \"indirect\": true");
}

/*
 * The fields of the value of a socket address length argument, followed
 * by extra, the meta-fields that describe how the value is updated.
 */
static inline void
jsonl_addrlen(unsigned int len, const char *extra)
{
	printf("\"type\": \"decimal\", \"raw\": \"%u\"", len);
	jsonl_indirect();
	printf("%s", extra);
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

/* Likewise, with the raw value formatted as a signed decimal. */
static inline void
jsonl_const_decimal(int64_t val, const char *sym, const char *sym_pattern)
{
	printf("\"type\": \"const\", \"raw\": \"%" PRId64 "\"", val);
	if (sym)
		printf(", \"sym\": \"%s\"", sym);
	else if (sym_pattern)
		printf(", \"sym_pattern\": \"%s\"", sym_pattern);
}

/* The name of val in xlat, or NULL if it has no name. */
static inline const char *
jsonl_xlat_sym(const struct xlat *xlat, uint64_t val)
{
	for (size_t i = 0; i < xlat->size; ++i)
		if (xlat->data[i].str && xlat->data[i].val == val)
			return xlat->data[i].str;
	return NULL;
}

/*
 * The "index" meta-field of an explicitly indexed array element, a decimal
 * const named by syms[idx], followed by the separator of the element value.
 */
static inline void
jsonl_index_const(uint64_t idx, const char *const *syms, size_t nsyms,
		  const char *sym_pattern)
{
	printf("\"index\": {");
	jsonl_const(idx, false, idx < nsyms ? syms[idx] : NULL,
		    idx < nsyms ? NULL : sym_pattern);
	printf("}, ");
}

/* The fields of an errno value: the number as given and its name. */
static inline void
jsonl_errno(int raw)
{
	printf("\"type\": \"errno\", \"raw\": \"%d\", \"sym\": \"%s\"",
	       raw, errno2name(raw < 0 ? -raw : raw));
}

/* The fields of a uid or gid value of the given type; -1 is printed signed. */
static inline void
jsonl_id(const char *type, uint32_t id)
{
	if (id == -1U)
		printf("\"type\": \"%s\", \"raw\": \"-1\"", type);
	else
		printf("\"type\": \"%s\", \"raw\": \"%u\"", type, id);
}

/* The fields of a uid value. */
static inline void
jsonl_uid(uint32_t uid)
{
	jsonl_id("uid", uid);
}

/* The fields of a gid value. */
static inline void
jsonl_gid(uint32_t gid)
{
	jsonl_id("gid", gid);
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
 * The fields of a ticks value: "type": "ticks", "raw": "<raw>", the clock
 * frequency "hz", and the decimal "precision".
 */
static inline void
jsonl_ticks_hz(uint64_t raw, long hz, unsigned int precision)
{
	printf("\"type\": \"ticks\", \"raw\": \"%" PRIu64 "\", "
	       "\"hz\": \"%ld\", \"precision\": \"%u\"", raw, hz, precision);
}

/*
 * Likewise, with "hz" and "precision" derived from _SC_CLK_TCK
 * the same way print_clock_t does.
 */
static inline void
jsonl_ticks(uint64_t raw)
{
	const long hz = sysconf(_SC_CLK_TCK);
	unsigned int precision = 0;

	for (long t = hz - 1; t > 0 && precision < 9; t /= 10)
		++precision;

	jsonl_ticks_hz(raw, hz, precision);
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

/* The fields of a timespec_t value. */
static inline void
jsonl_timespec(long long sec, long long nsec)
{
	printf("\"type\": \"timespec_t\", \"fields\": {\"tv_sec\": \"%lld\","
	       " \"tv_nsec\": \"%lld\"}", sec, nsec);
}

/* The fields of a timeval_t value. */
static inline void
jsonl_timeval(long long sec, unsigned long long usec)
{
	printf("\"type\": \"timeval_t\", \"fields\": {\"tv_sec\": \"%lld\","
	       " \"tv_usec\": \"%llu\"}", sec, usec);
}

/* The fields of a pid value of the given type: tid, tgid, pgid, or sid. */
static inline void
jsonl_pid(const char *type, int pid)
{
	printf("\"type\": \"%s\", \"raw\": \"%d\"", type, pid);
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

/* The fields of a path value of len bytes, optionally marked truncated. */
static inline void
jsonl_path_value_n(const void *p, size_t len, bool truncated)
{
	printf("\"type\": \"path\", \"value\": ");
	jsonl_bytes_n(p, len);
	if (truncated)
		jsonl_truncated();
}

/* The fields of a path value of a NUL-terminated path. */
static inline void
jsonl_path_value(const char *path)
{
	jsonl_path_value_n(path, strlen(path), false);
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

/* The fields of a char value. */
static inline void
jsonl_char(unsigned char c)
{
	printf("\"type\": \"char\", \"raw\": \"%u\"", c);
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

/* Like jsonl_flags_open, for a flags value printed without a default name. */
static inline void
jsonl_flags_open_dflt_null(uint64_t raw)
{
	printf("\"type\": \"flags\", \"raw\": ");
	jsonl_xlat_raw(raw);
	printf(", \"groups\": [{\"raw\": ");
	jsonl_xlat_raw(raw);
	printf(", \"group_type\": \"flags\", \"dflt_null\": \"true\","
	       " \"elems\": [");
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
 * Open a flags value of several groups, up to the groups array:
 * "type": "flags", "raw": "<raw>", "groups": [
 */
static inline void
jsonl_flags_groups_open(uint64_t raw)
{
	printf("\"type\": \"flags\", \"raw\": ");
	jsonl_xlat_raw(raw);
	printf(", \"groups\": [");
}

/* Open a group up to its elems array: {"raw": "<raw>", ..., "elems": [ */
static inline void
jsonl_flags_group_open(uint64_t raw, bool first)
{
	printf("%s{\"raw\": ", first ? "" : ", ");
	jsonl_xlat_raw(raw);
	printf(", \"group_type\": \"flags\", \"elems\": [");
}

/* Close the elems array and the group: ]}. */
static inline void
jsonl_flags_group_close(void)
{
	printf("]}");
}

/* Close the groups array of a flags value: ]. */
static inline void
jsonl_flags_groups_close(void)
{
	printf("]");
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

/* Likewise, with the names taken from the entries of xlat. */
static inline void
jsonl_flags_xlat(const struct xlat *xlat, uint64_t raw, const char *dflt)
{
	uint64_t rem = raw;
	bool first = true;

	jsonl_flags_open(raw);
	for (size_t i = 0; i < xlat->size; ++i) {
		const uint64_t val = xlat->data[i].val;

		if (!xlat->data[i].str ||
		    (val ? (rem & val) != val : raw != 0))
			continue;
		jsonl_flags_const(val, xlat->data[i].str, first);
		rem &= ~val;
		first = false;
	}
	if (rem)
		jsonl_flags_remainder(rem, first ? dflt : NULL, first);
	jsonl_flags_close();
}

/* A struct field with an unsigned value. */
static inline void
jsonl_field_unsigned(const char *name, bool first, uint64_t val)
{
	jsonl_field_open(name, first);
	jsonl_unsigned(val);
	jsonl_field_close();
}

/* A struct field with a char value. */
static inline void
jsonl_field_char(const char *name, bool first, unsigned char c)
{
	jsonl_field_open(name, first);
	jsonl_char(c);
	jsonl_field_close();
}

/* A struct field with a decimal value. */
static inline void
jsonl_field_decimal(const char *name, bool first, int64_t val)
{
	jsonl_field_open(name, first);
	jsonl_decimal(val);
	jsonl_field_close();
}

/* A struct field with a pid value of the given type. */
static inline void
jsonl_field_pid(const char *name, bool first, const char *type, int pid)
{
	jsonl_field_open(name, first);
	jsonl_pid(type, pid);
	jsonl_field_close();
}

/* A struct field with a hex value. */
static inline void
jsonl_field_hex(const char *name, bool first, uint64_t val)
{
	jsonl_field_open(name, first);
	jsonl_hex(val);
	jsonl_field_close();
}

/* A struct field with a const value whose raw value is an xlat value. */
static inline void
jsonl_field_const(const char *name, bool first, uint64_t val,
		  const char *sym, const char *sym_pattern)
{
	jsonl_field_open(name, first);
	jsonl_const(val, true, sym, sym_pattern);
	jsonl_field_close();
}

/* A struct field with a table-driven flags value. */
static inline void
jsonl_field_flags(const char *name, bool first, uint64_t raw,
		  const struct strval32 *known, size_t n, const char *dflt)
{
	jsonl_field_open(name, first);
	jsonl_flags(raw, known, n, dflt);
	jsonl_field_close();
}

/* A struct field with a flags value that consists of one known flag. */
static inline void
jsonl_field_flag(const char *name, bool first, uint32_t val, const char *sym)
{
	const struct strval32 flag = { val, sym };

	jsonl_field_flags(name, first, val, &flag, 1, NULL);
}

/* A struct field with an ifindex value. */
static inline void
jsonl_field_ifindex(const char *name, bool first, unsigned int index,
		    const char *ifname)
{
	jsonl_field_open(name, first);
	jsonl_ifindex(index, ifname);
	jsonl_field_close();
}

/* A struct field with a string value of a NUL-terminated string. */
static inline void
jsonl_field_string(const char *name, bool first, const char *str)
{
	jsonl_field_open(name, first);
	jsonl_string_value(str, strlen(str), false);
	jsonl_field_close();
}

#endif /* !STRACE_TESTS_JSONL_H */
