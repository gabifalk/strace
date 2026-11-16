/*
 * Copyright (c) 2016-2017 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2017-2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"
#include "hyperlink.h"
#include "print_fields.h"
#include "print_utils.h"

static void
tprints_object_field_bytes(const char *field, const char *raw, size_t len)
{
	const unsigned char *bytes = (const unsigned char *) raw;

	tprints_object_field_begin(field);
	tprint_list_begin();
	for (size_t i = 0; i < len; ++i) {
		tprint_list_element_begin();
		STRACE_PRINTF("%u", bytes[i]);
		tprint_list_element_end();
	}
	tprint_list_end();
	tprint_object_field_end();
}

void
tprint_struct_begin(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("{");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_struct_next(void)
{
	tprint_object_end();
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(", ");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_struct_end(void)
{
	tprint_object_end();
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("}");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_union_begin(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("{");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_union_next(void)
{
	tprint_object_end();
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(", ");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_union_end(void)
{
	tprint_object_end();
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("}");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_array_begin(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("[");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_array_next(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(", ");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_array_end(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("]");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_array_element_fetch_failed(unsigned long long addr)
{
	if (structured_output)
		return;

	tprint_more_data_follows();
	printaddr_comment(addr);
}

void
tprint_array_index_begin(void)
{
	if (structured_output) {
		tprints_object_field_begin("index");
		tprint_object_begin();
	} else {
		STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
		STRACE_PRINTS("[");
		STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
	}
}

void
tprint_array_index_equal(void)
{
	if (structured_output) {
		tprint_object_end();
		tprint_object_field_end();
	} else {
		STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
		STRACE_PRINTS("]=");
		STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
	}
}

void
tprints_syscall_name(const char *name)
{
	if (!name) {
		STRACE_PRINTF("%s", "system call");
		return;
	}

	if (hyperlink_mode != HYPERLINK_ALWAYS) {
		STRACE_PRINTF("%s", name);
		return;
	}

	char *url = make_hyperlink_url_for_syscall(name);
	if (!url) {
		STRACE_PRINTF("%s", name);
		return;
	}

	tprint_hyperlink_begin(url);
	STRACE_PRINTF("%s", name);
	tprint_hyperlink_end();
	free(url);
}

void
tprints_arg_begin(const char *name)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_SYSCALL);
	tprints_syscall_name(name);
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("(");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_arg_next(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(", ");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_arg_end(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(")");
	STRACE_PRINT_COLOR_SEQ(COLOR_RESET);
}

void
tprints_arg_name_unconditionally(const char *name)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGNAME);
	STRACE_PRINTF("%s", name);
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("=");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprints_arg_next_name_unconditionally(const char *name)
{
	tprint_arg_next();
	tprints_arg_name_unconditionally(name);
}

void
tprints_arg_name(const char *name)
{
	if (Nflag)
		tprints_arg_name_unconditionally(name);
}

void
tprints_arg_next_name(const char *name)
{
	tprint_arg_next();
	tprints_arg_name(name);
}

void
tprints_fn_begin(const char *name)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_CALL);
	STRACE_PRINTF("%s", name);
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("(");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_fn_next(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(", ");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_fn_end(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(")");
	STRACE_PRINT_COLOR_SEQ(COLOR_RESET);
}

void
tprint_bitset_begin(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("[");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_bitset_next(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(" ");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_bitset_end(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("]");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_comment_begin(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_COMMENT);
	STRACE_PRINTS(" /* ");
}

void
tprint_comment_end(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_COMMENT);
	STRACE_PRINTS(" */");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_indirect_begin(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("[");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_indirect_end(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("]");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_attribute_begin(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("[");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_attribute_end(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("]");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_associated_info_begin(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("<");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_associated_info_end(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(">");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprints_fd_info_string(const char *type, const char *str)
{
	if (structured_output) {
		tprints_object_field_begin("fd_info");
		tprint_object_begin();
		tprints_object_type(type);
		tprints_object_field_bytes(strcmp(type, "socket") ? "path"
								  : "details",
					   str, strlen(str));
		tprint_object_end();
		tprint_object_field_end();
	} else {
		tprint_associated_info_begin();
		STRACE_PRINTS(str);
		tprint_associated_info_end();
	}
}

void
tprint_fd_info_dev(const char *path, const char *kind, unsigned int major,
		   unsigned int minor, int tty_index, const char *tun_backend)
{
	tprint_associated_info_begin();
	print_quoted_string_ex(path, strlen(path),
			       QUOTE_OMIT_LEADING_TRAILING_QUOTES, "<>");
	tprint_associated_info_begin();
	STRACE_PRINTF("%s %u:%u", kind, major, minor);
	if (tty_index >= 0) {
		STRACE_PRINTF(" @/dev/pts/%d", tty_index);
	} else if (tun_backend[0]) {
		STRACE_PRINTS(" @");
		print_quoted_string_ex(tun_backend, strlen(tun_backend),
				       QUOTE_OMIT_LEADING_TRAILING_QUOTES,
				       "<>");
	}
	tprint_associated_info_end();
	tprint_associated_info_end();
}

void
tprints_assoc_type_string(const char *type, const char *str, size_t len,
			  bool deleted)
{
	if (structured_output && !strcmp(type, "fd_info")) {
		tprints_object_field_begin(type);
		tprint_object_begin();
		tprints_object_type("path");
		tprints_object_field_bytes("path", str, len);
		if (deleted)
			tprints_object_field_bool("deleted", true);
		tprint_object_end();
		tprint_object_field_end();
	} else {
		tprint_associated_info_begin();
		print_quoted_string_ex(str, len,
				       QUOTE_OMIT_LEADING_TRAILING_QUOTES,
				       "<>");
		tprint_associated_info_end();

		if (deleted)
			STRACE_PRINTS("(deleted)");
	}
}

void
tprint_more_data_follows(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("...");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_value_changed(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(" => ");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_alternative_value(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(" or ");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_unavailable(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_ERROR);
	STRACE_PRINTS("???");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_struct_addr(unsigned long long addr)
{
	if (structured_output)
		tprintf_object_field_string("addr", "%#llx", addr);
	else
		tprintf_comment("%#llx", addr);
}

void
tprint_flags_or(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("|");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_newline(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_RESET);
	STRACE_PRINTS("\n");
}

void
tprints_field_name(const char *name)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGNAME);
	STRACE_PRINTF("%s", name);
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("=");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
	tprint_object_begin();
}

void
tprints_xlat_sym(const char *str)
{
	if (!str || !*str)
		return;

	if (structured_output)
		tprints_object_field_string("sym", str);
	else
		tprints_comment(str);
}

void
tprints_xlat_dflt(const char *dflt)
{
	if (!dflt || !*dflt)
		return;

	if (structured_output)
		tprints_object_field_string("sym_pattern", dflt);
	else
		tprints_comment(dflt);
}

void
tprint_sun_path_abstract(void)
{
	if (structured_output)
		tprints_object_field_bool("abstract", true);
	else
		STRACE_PRINTS("@");
}

void
tprints_quoted_string(const char *quoted, const char *raw, unsigned int size,
		      bool nul_terminated)
{
	if (structured_output)
		tprints_object_field_bytes("value", raw,
					   nul_terminated ? strnlen(raw, size)
							  : size);
	else
		STRACE_PRINTS(quoted);
}

void
tprint_sysret_begin(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("=");
	STRACE_PRINT_COLOR_SEQ(COLOR_RESET);
}

void
tprints_sysret_next(const char *name)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_RESET);
	tprint_space();
	if (color_is_enabled && name) {
		if (!strcmp(name, "error") ||
		    !strcmp(name, "errno") ||
		    !strcmp(name, "strerror")) {
			STRACE_PRINT_COLOR_SEQ(COLOR_ERROR);
			return;
		}
	}
	STRACE_PRINT_COLOR_SEQ(COLOR_RETVAL);
}

void
tprints_sysret_string(const char *name, const char *str)
{
	tprints_sysret_next(name);
	if (structured_output)
		tprints_object_field_string(name, str);
	else
		STRACE_PRINTF("(%s)", str);
}

static const char *
timescale_to_unit(int scale)
{
	switch (scale) {
	case 1000000000:
		return "seconds";
	case 1000000:
		return "milliseconds";
	case 1000:
		return "microseconds";
	case 1:
		return "nanoseconds";
	default:
		return NULL;
	}
}

void
tprint_negated_begin(long long raw)
{
	if (structured_output) {
		tprints_object_type("negated");
		tprintf_object_field_string("raw", "%lld", raw);
		tprints_object_field_begin("value");
		tprint_object_begin();
	} else {
		tprints_string("-");
	}
}

void
tprint_negated_end(void)
{
	if (structured_output) {
		tprint_object_end();
		tprint_object_field_end();
	}
}

void
tprint_var_count(unsigned long long addr, unsigned int count,
		 bool unterminated)
{
	if (structured_output) {
		tprints_object_type("var_count");
		tprintf_object_field_string("raw", "%#llx", addr);
		tprintf_object_field_string("count", "%u", count);
		if (unterminated)
			tprints_object_field_bool("unterminated", true);
	} else {
		printaddr64(addr);
		tprintf_comment("%u var%s%s", count, count == 1 ? "" : "s",
				unterminated ? ", unterminated" : "");
	}
}

void
tprint_char(unsigned char c)
{
	if (structured_output) {
		tprints_object_type("char");
		tprintf_object_field_string("raw", "%u", c);
	} else {
		print_char(c, SCF_QUOTES | SCF_ESC_WS);
	}
}

void
tprint_extra_data_unavailable(void)
{
	if (structured_output && tprint_struct_fields_end())
		tprints_object_field_bool("rest_unreadable", true);
	else
		tprint_unavailable();
}

static bool extra_data_field;

void
tprint_extra_data_begin(unsigned int offset, unsigned int size)
{
	if (structured_output) {
		extra_data_field = tprint_struct_fields_end();
		if (extra_data_field) {
			tprints_object_field_begin("extra_data");
			tprint_object_begin();
		}
		tprints_object_type("bytes");
		tprintf_object_field_string("offset", "%u", offset);
		tprintf_object_field_string("size", "%u", size);
	} else {
		tprintf_string("/* bytes %u..%u */ ", offset,
			       offset + size - 1);
	}
}

void
tprint_extra_data_end(bool truncated)
{
	if (structured_output) {
		if (truncated)
			tprints_object_field_bool("truncated", true);
		if (extra_data_field) {
			tprint_object_end();
			tprint_object_field_end();
			extra_data_field = false;
		}
	} else if (truncated) {
		tprint_more_data_follows();
	}
}

void
tprint_addr(unsigned long long addr)
{
	PRINT_VAL_ADDR(addr);
}

void
tprint_syscall_duration(long long sec, long nsec)
{
	if (structured_output) {
		const char *unit = timescale_to_unit(Tflag_scale);

		if (unit) {
			long long factor = 1000000000LL / Tflag_scale;

			tprintf_object_field_string("time", "%lld",
						    sec * factor
						    + nsec / Tflag_scale);
			tprints_object_field_string("time_unit", unit);
		}
	} else {
		tprints_sysret_next("time");
		tprint_associated_info_begin();
		PRINT_VAL_D(sec);
		if (Tflag_width) {
			tprintf_string(".%0*ld", Tflag_width,
				       nsec / Tflag_scale);
		}
		tprint_associated_info_end();
	}
}

void
tprint_sysret_end(void)
{
}

void
tprint_sysret_errno(unsigned long u_error, const char *sym, const char *str)
{
	if (sym) {
		tprints_sysret_next("error");
		STRACE_PRINTS(sym);
		tprints_sysret_string("strerror", str);
	} else {
		tprints_sysret_next("strerror");
		STRACE_PRINTF("(errno %lu)", u_error);
	}
}

void
tprint_syscall_exit_line_end(void)
{
	if (!structured_output) {
		tprint_event_end();
		tprint_newline();
	}
}

void
tprint_syscall_exit_event_end(void)
{
	if (structured_output) {
		tprint_event_end();
		tprint_newline();
	}
}

static struct {
	bool iov;
	bool fetch_failed;
	unsigned long long size;
	unsigned long long addr;
} io_dump;

void
tprint_io_dump_begin(void)
{
	if (structured_output) {
		if (io_dump.iov)
			tprint_list_element_begin();
		else
			tprints_object_field_begin("io_dump");
		tprint_object_begin();
		tprints_object_type("bytes");
		tprints_object_field_begin("value");
		tprint_list_begin();
	}
}

void
tprint_io_dump_line(const char *text, unsigned long long offset,
		    int offset_width, const void *data, unsigned int size)
{
	if (structured_output) {
		const unsigned char *const bytes = data;

		for (unsigned int i = 0; i < size; ++i) {
			tprint_list_element_begin();
			STRACE_PRINTF("%u", bytes[i]);
			tprint_list_element_end();
		}
	} else {
		tprintf_string(" | %0*llx  %s |\n", offset_width, offset,
			       text);
	}
}

void
tprint_io_dump_fetch_failed(unsigned long long size,
			    unsigned long long fetch_size, int pid,
			    unsigned long long addr, unsigned long long fetched)
{
	if (structured_output) {
		io_dump.fetch_failed = true;
		io_dump.size = size;
		io_dump.addr = addr;
	} else if (fetched) {
		/*
		 * Don't silently abort if something has been printed already.
		 */
		tprintf_string(" | <Cannot fetch %llu byte%s from pid %d"
			       " @%#llx>\n",
			       fetch_size, fetch_size == 1 ? "" : "s",
			       pid, addr);
	}
}

void
tprint_io_dump_end(void)
{
	if (structured_output) {
		tprint_list_end();
		tprint_object_field_end();
		if (io_dump.fetch_failed) {
			tprintf_object_field_string("size", "%llu",
						    io_dump.size);
			tprintf_object_field_string("fetch_failed_addr",
						    "%#llx", io_dump.addr);
			io_dump.fetch_failed = false;
		}
		tprint_object_end();
		if (io_dump.iov)
			tprint_list_element_end();
		else
			tprint_object_field_end();
	}
}

void
tprint_io_dump_iov_begin(void)
{
	if (structured_output) {
		tprints_object_field_begin("io_dump");
		tprint_object_begin();
		tprint_array_begin();
		io_dump.iov = true;
	}
}

void
tprint_io_dump_iov_buffer(unsigned long long size, unsigned int index)
{
	if (!structured_output) {
		/*
		 * Include the buffer number to make it easy to match up
		 * the trace with the source.
		 */
		tprintf_string(" * %llu bytes in buffer %u\n", size, index);
	}
}

void
tprint_io_dump_iov_end(void)
{
	if (structured_output) {
		io_dump.iov = false;
		tprint_array_end();
		tprint_object_end();
		tprint_object_field_end();
	}
}
