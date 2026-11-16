/*
 * Copyright (c) 2016-2017 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2017-2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include "hyperlink.h"
#include "print_fields.h"
#include "print_utils.h"
#include "wait.h"
#include "xstring.h"

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
tprint_pair_begin(void)
{
	if (structured_output) {
		tprints_object_field_string("type", "pair");
		tprints_object_field_begin("elems");
		tprint_list_begin();
	} else {
		tprint_array_begin();
	}
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
tprint_fd_set_begin(void)
{
	if (structured_output) {
		tprints_object_type("fd_set");
		tprints_object_field_begin("elems");
		tprint_list_begin();
	} else {
		tprint_bitset_begin();
	}
}

void
tprint_fd_set_next(void)
{
	if (structured_output)
		tprint_array_next();
	else
		tprint_bitset_next();
}

void
tprint_fd_set_end(void)
{
	if (structured_output) {
		tprint_list_end();
		tprint_object_field_end();
	} else {
		tprint_bitset_end();
	}
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
tprint_cpu_set_begin(void)
{
	if (structured_output) {
		tprints_object_type("cpu_set_t");
		tprints_object_field_begin("elems");
		tprint_list_begin();
	} else {
		tprint_bitset_begin();
	}
}

void
tprint_cpu_set_next(void)
{
	if (!structured_output)
		tprint_bitset_next();
}

void
tprint_cpu_set_cpu(int cpu)
{
	if (structured_output) {
		tprint_list_element_begin();
		char buf[sizeof(cpu) * 3];

		xsprintf(buf, "%d", cpu);
		tprints_string_value(buf);
		tprint_list_element_end();
	} else {
		PRINT_VAL_D(cpu);
	}
}

void
tprint_cpu_set_end(void)
{
	if (structured_output) {
		tprint_array_end();
	} else {
		tprint_bitset_end();
	}
}

static unsigned long long
bitset_word(const void *buf, unsigned int idx)
{
	if (current_wordsize == sizeof(uint64_t)) {
		uint64_t word;

		memcpy(&word, (const char *) buf + idx * sizeof(word),
		       sizeof(word));
		return word;
	} else {
		uint32_t word;

		memcpy(&word, (const char *) buf + idx * sizeof(word),
		       sizeof(word));
		return word;
	}
}

bool
tprint_bitset_mask_begin(const void *buf, unsigned int size)
{
	bool decoded = true;

	if (structured_output) {
		unsigned int nwords = size / current_wordsize;
		char raw[sizeof("0x") + 2 * size];
		char *p = raw;

		while (nwords > 0 && !bitset_word(buf, nwords - 1))
			--nwords;

		tprints_object_type("bitset");
		if (nwords) {
			p = xappendstr(raw, p, "%#llx",
				       bitset_word(buf, nwords - 1));
			for (unsigned int i = nwords - 1; i > 0; --i)
				p = xappendstr(raw, p, "%0*llx",
					       (int) (2 * current_wordsize),
					       bitset_word(buf, i - 1));
			tprints_object_field_string("raw", raw);
		} else {
			tprints_object_field_string("raw", "0");
		}
		tprints_object_field_begin("elems");
		tprint_list_begin();
	} else if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_RAW) {
		decoded = false;
	} else {
		tprint_bitset_begin();
	}

	return decoded;
}

void
tprint_bitset_raw_begin(unsigned int raw, bool inverted)
{
	if (structured_output) {
		tprints_object_type("bitset");
		tprintf_object_field_string("raw", "%#x", raw);
		if (inverted)
			tprints_object_field_bool("inverted", true);
		tprints_object_field_begin("elems");
		tprint_list_begin();
	} else {
		if (inverted)
			tprints_string("~");
		tprint_bitset_begin();
	}
}

void
tprint_bitset_mask_next(void)
{
	if (structured_output)
		tprint_array_next();
	else
		tprint_bitset_next();
}

void
tprint_bitset_mask_end(void)
{
	if (structured_output)
		tprint_array_end();
	else
		tprint_bitset_end();
}

bool
tprint_bitset_words_begin(const void *buf, unsigned int size)
{
	bool printed = false;

	if (structured_output) {
		const unsigned int nwords = size / current_wordsize;

		tprints_object_field_begin("words");
		tprint_object_begin();
		tprint_array_begin();
		for (unsigned int i = 0; i < nwords; ++i) {
			tprint_list_element_begin();
			tprint_object_begin();
			tprints_object_type("hex");
			tprintf_object_field_string("raw", "%#llx",
						    bitset_word(buf, i));
			tprint_object_end();
			tprint_list_element_end();
		}
		tprint_array_end();
		tprint_object_end();
		tprint_object_field_end();
	} else {
		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
			tprint_comment_begin();
		printed = xlat_verbose(xlat_verbosity) != XLAT_STYLE_ABBREV;
	}

	return printed;
}

void
tprint_bitset_words_end(void)
{
	if (!structured_output
	    && xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
		tprint_comment_end();
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
	if (structured_output) {
		tprints_object_field_begin("fd_info");
		tprint_object_begin();
		tprints_object_type("dev_t");
		tprints_object_field_bytes("path", path, strlen(path));
		tprints_object_field_string("kind", kind);
		tprintf_object_field_string("major", "%u", major);
		tprintf_object_field_string("minor", "%u", minor);
		if (tty_index >= 0) {
			tprintf_object_field_string("tty_index", "%d",
						    tty_index);
		}
		tprint_object_end();
		tprint_object_field_end();
	} else {
		tprint_associated_info_begin();
		print_quoted_string_ex(path, strlen(path),
				       QUOTE_OMIT_LEADING_TRAILING_QUOTES,
				       "<>");
		tprint_associated_info_begin();
		STRACE_PRINTF("%s %u:%u", kind, major, minor);
		if (tty_index >= 0) {
			STRACE_PRINTF(" @/dev/pts/%d", tty_index);
		} else if (tun_backend[0]) {
			STRACE_PRINTS(" @");
			print_quoted_string_ex(
				tun_backend, strlen(tun_backend),
				QUOTE_OMIT_LEADING_TRAILING_QUOTES, "<>");
		}
		tprint_associated_info_end();
		tprint_associated_info_end();
	}
}

void
tprint_fd_info_pidfd(int pid)
{
	if (structured_output) {
		tprints_object_field_begin("fd_info");
		tprint_object_begin();
		tprints_object_type("pidfd");
		tprints_object_field_begin("pid");
		tprint_object_begin();
		printpid(NULL, pid, PT_TID);
		tprint_object_end();
		tprint_object_field_end();
		tprint_object_end();
		tprint_object_field_end();
	} else {
		tprint_associated_info_begin();
		STRACE_PRINTS("pid:");
		/*
		 * The pid translation is not needed because
		 * the pid is in strace's namespace.
		 */
		printpid(NULL, pid, PT_TID);
		tprint_associated_info_end();
	}
}

void
tprints_fd_info_signalfd(const char *sigmask)
{
	if (structured_output) {
		tprints_object_field_begin("fd_info");
		tprint_object_begin();
		tprints_object_type("signalfd");
		tprints_object_field_string("sigmask", sigmask);
		tprint_object_end();
		tprint_object_field_end();
	} else {
		tprint_associated_info_begin();
		STRACE_PRINTS("signalfd:");
		STRACE_PRINTS(sigmask);
		tprint_associated_info_end();
	}
}

void
tprint_fd_info_eventfd(const char *count, int id, int semaphore)
{
	if (structured_output) {
		tprints_object_field_begin("fd_info");
		tprint_object_begin();
		tprints_object_type("eventfd");
		tprintf_object_field_string("count",
					    count[0] != '0' ? "0x%s" : "%s",
					    count);
		if (id != -1) {
			tprintf_object_field_string("id", "%u", id);
			if (semaphore != -1) {
				tprintf_object_field_string("semaphore", "%u",
							    semaphore);
			}
		}
		tprint_object_end();
		tprint_object_field_end();
	} else {
		tprint_associated_info_begin();
		tprint_struct_begin();
		tprints_field_name("eventfd-count");
		if (count[0] != '0')
			STRACE_PRINTS("0x");
		STRACE_PRINTS(count);

		if (id != -1) {
			tprint_struct_next();
			tprints_field_name("eventfd-id");
			PRINT_VAL_U(id);

			if (semaphore != -1) {
				tprint_struct_next();
				tprints_field_name("eventfd-semaphore");
				PRINT_VAL_U(semaphore);
			}
		}

		tprint_struct_end();
		tprint_associated_info_end();
	}
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
tprints_pid_info(const char *comm, int strace_pid)
{
	if (structured_output) {
		if (comm[0] || strace_pid) {
			tprints_object_field_begin("pid_info");
			tprint_object_begin();
			if (comm[0]) {
				tprints_object_field_bytes("comm", comm,
							   strlen(comm));
			}
			if (strace_pid) {
				tprintf_object_field_string("strace_pid", "%d",
							    strace_pid);
			}
			tprint_object_end();
			tprint_object_field_end();
		}
	} else {
		if (comm[0])
			tprints_assoc_type_string("pid_info", comm,
						  strlen(comm), false);
		if (strace_pid)
			tprintf_comment("%d in strace's PID NS", strace_pid);
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
tprint_unavailable_error(const char *msg)
{
	tprint_unavailable();
	if (structured_output)
		tprints_object_field_string("decode_error", msg);
	else
		tprints_comment(msg);
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
tprint_struct_decode_error(const char *msg)
{
	tprints_comment(msg);
}

void
tprint_flags_or(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("|");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_flags_groups_begin(unsigned long long raw)
{
	if (structured_output) {
		tprints_object_type("flags");
		tprintf_object_field_string("raw", "%#llx", raw);
		tprints_object_field_begin("groups");
		tprint_list_begin();
	} else {
		tprint_flags_begin();
	}
}

void
tprint_flags_groups_next(void)
{
	if (!structured_output)
		tprint_flags_or();
}

void
tprint_flags_groups_end(void)
{
	if (structured_output) {
		tprint_list_end();
		tprint_object_field_end();
	} else {
		tprint_flags_end();
	}
}

static void
tprints_flags_typed_group_begin(unsigned long long raw, const char *group_type)
{
	if (structured_output) {
		tprint_list_element_begin();
		tprint_object_begin();
		tprintf_object_field_string("raw", "%#llx", raw);
		tprints_object_field_string("group_type", group_type);
		tprints_object_field_begin("elems");
		tprint_list_begin();
		tprint_list_element_begin();
		tprint_object_begin();
		tprint_flags_element_mark();
	}
}

void
tprint_flags_group_begin(unsigned long long raw)
{
	tprints_flags_typed_group_begin(raw, "flags");
}

void
tprint_flags_shift_group_begin(unsigned long long raw)
{
	tprints_flags_typed_group_begin(raw, "shift");
}

void
tprint_flags_mode_group_begin(unsigned long long raw)
{
	tprints_flags_typed_group_begin(raw, "mode");
}

void
tprint_flags_group_end(void)
{
	if (structured_output) {
		tprint_object_end();
		tprint_list_element_end();
		tprint_list_end();
		tprint_object_field_end();
		tprint_object_end();
		tprint_list_element_end();
	}
}

void
tprint_flags_value_begin(unsigned long long raw)
{
	if (structured_output) {
		tprints_object_type("flags");
		tprintf_object_field_string("raw", "%#llx", raw);
		tprints_object_field_begin("groups");
		tprint_list_begin();
		tprint_list_element_begin();
		tprint_object_begin();
		tprintf_object_field_string("raw", "%#llx", raw);
		tprints_object_field_string("group_type", "flags");
		tprints_object_field_begin("elems");
		tprint_list_begin();
		tprint_list_element_begin();
		tprint_object_begin();
		tprint_flags_element_mark();
	} else {
		tprint_flags_begin();
	}
}

void
tprint_flags_value_or(void)
{
	if (structured_output) {
		tprint_object_end();
		tprint_list_element_end();
		tprint_list_element_begin();
		tprint_object_begin();
		tprint_flags_element_mark();
	} else {
		tprint_flags_or();
	}
}

void
tprint_flags_value_end(void)
{
	if (structured_output) {
		tprint_object_end();
		tprint_list_element_end();
		tprint_list_end();
		tprint_object_field_end();
		tprint_object_end();
		tprint_list_element_end();
		tprint_list_end();
		tprint_object_field_end();
	} else {
		tprint_flags_end();
	}
}

bool
tprint_xlat_flags_value_begin(unsigned long long raw)
{
	if (structured_output) {
		tprint_flags_value_begin(raw);
	} else {
		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_ABBREV)
			PRINT_VAL_X(raw);

		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_RAW)
			return false;

		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
			tprint_comment_begin();

		tprint_flags_begin();
	}
	return true;
}

void
tprint_xlat_flags_value_end(void)
{
	if (structured_output) {
		tprint_flags_value_end();
	} else {
		tprint_flags_end();

		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
			tprint_comment_end();
	}
}

void
tprints_flags_value_const(unsigned long long val, const char *sym)
{
	if (structured_output) {
		tprints_object_type("const");
		tprintf_object_field_string("raw", "%#llx", val);
		tprints_object_field_string("sym", sym);
	} else {
		tprints_string(sym);
	}
}

void
tprints_shift_value(unsigned long long raw, unsigned long long val,
		    const char *shift)
{
	if (structured_output) {
		tprints_object_type("shift");
		tprintf_object_field_string("raw", "%#llx", raw);
		tprintf_object_field_string("value", "%llu", val);
		tprints_object_field_string("shift", shift);
	} else {
		tprint_shift_begin();
		PRINT_VAL_U(val);
		tprint_shift();
		tprints_string(shift);
		tprint_shift_end();
	}
}

void
tprints_shift_value_xlat_u(unsigned long long raw, unsigned long long val,
			   unsigned int amount, const char *shift)
{
	if (structured_output) {
		tprints_shift_value(raw, val, shift);
	} else {
		tprint_shift_begin();
		PRINT_VAL_U(val);
		tprint_shift();
		print_xlat_ex(amount, shift, XLAT_STYLE_FMT_U);
		tprint_shift_end();
	}
}

void
tprint_shift_value_begin(unsigned long long raw)
{
	if (structured_output) {
		tprints_object_type("shift");
		tprintf_object_field_string("raw", "%#llx", raw);
		tprints_object_field_begin("value");
		tprint_object_begin();
	} else {
		tprint_shift_begin();
	}
}

void
tprint_shift_value_end_u(unsigned int amount)
{
	if (structured_output) {
		tprint_object_end();
		tprint_object_field_end();
		tprintf_object_field_string("shift", "%u", amount);
	} else {
		tprint_shift();
		PRINT_VAL_U(amount);
		tprint_shift_end();
	}
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
tprints_unnamed_field_name(const char *name)
{
	if (structured_output)
		tprints_field_name(name);
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
tprints_xlat_const(uint64_t val, enum xlat_style style, const char *str)
{
	if (!str || !*str)
		return;

	if (structured_output) {
		tprint_type_const();
		tprints_object_field_string("raw", sprint_xlat_val(val, style));
		tprints_object_field_string("sym", str);
	} else {
		tprint_color_seq(COLOR_CONST);
		tprints_string(str);
		tprint_color_seq(COLOR_ARGVAL);
	}
}

void
tprint_xlat_flags_begin(uint64_t flags, const char *dflt,
			enum xlat_style style)
{
	if (structured_output) {
		if (!tprint_flags_element_open()) {
			tprints_object_type("flags");
			tprints_object_field_string("raw",
				sprint_xlat_val(flags, style));
			tprints_object_field_begin("groups");
			tprint_list_begin();
			tprint_list_element_begin();
			tprint_object_begin();
			tprints_object_field_string("raw",
				sprint_xlat_val(flags, style));
			tprints_object_field_string("group_type", "flags");
			if (!dflt)
				tprints_object_field_string("dflt_null",
							    "true");
			tprints_object_field_begin("elems");
			tprint_list_begin();
		}
	} else {
		if (xlat_verbose(style) == XLAT_STYLE_RAW) {
			if (flags || dflt)
				print_xlat_val(flags, style);
		} else if (xlat_verbose(style) == XLAT_STYLE_VERBOSE
			   && flags) {
			print_xlat_val(flags, style);
		}
	}
}

/*
 * Print the name `sym' of the constant `val' matched in `flags',
 * the `n'th one of them counting from zero.
 */
void
tprints_xlat_flags_sym(const char *sym, uint64_t val, uint64_t flags,
		       unsigned int n, enum xlat_style style)
{
	const bool need_comment = xlat_verbose(style) == XLAT_STYLE_VERBOSE;

	if (structured_output) {
		if (tprint_flags_element_open()) {
			if (n)
				tprint_flags_value_or();
			tprints_object_type("const");
			tprints_object_field_string("raw",
				sprint_xlat_val(val, style));
			tprints_object_field_string("sym", sym);
		} else {
			tprint_list_element_begin();
			tprint_object_begin();
			tprints_object_type("const");
			tprints_object_field_string("raw",
				sprint_xlat_val(val, style));
			tprints_object_field_string("sym", sym);
			tprint_object_end();
			tprint_list_element_end();
		}
	} else if (xlat_verbose(style) != XLAT_STYLE_RAW) {
		if (need_comment && !flags)
			PRINT_VAL_U(0);
		if (n)
			tprint_flags_or();
		else if (need_comment)
			tprint_comment_begin();
		if (need_comment)
			tprints_string(sym);
		else
			tprints_xlat_const(val, style, sym);
	}
}

/*
 * Finish printing `orig_flags' after `n' constants have been matched
 * and `flags' are left unmatched; return the number of items printed.
 */
int
tprint_xlat_flags_end(uint64_t orig_flags, uint64_t flags, unsigned int n,
		      const char *dflt, enum xlat_style style)
{
	if (structured_output) {
		const bool flattened = tprint_flags_element_open();

		if (flags) {
			if (flattened) {
				if (n)
					tprint_flags_value_or();
			} else {
				tprint_list_element_begin();
				tprint_object_begin();
			}
			tprints_object_type("remainder");
			tprints_object_field_string("raw",
						    sprint_xlat_val(flags,
								    style));
			if (dflt && !n)
				tprints_object_field_string("sym", dflt);
			if (!flattened) {
				tprint_object_end();
				tprint_list_element_end();
			}
			n++;
		}
		if (!flattened) {
			tprint_list_end();
			tprint_object_field_end();
			tprint_object_end();
			tprint_list_element_end();
			tprint_list_end();
			tprint_object_field_end();
		}
	} else if (xlat_verbose(style) == XLAT_STYLE_RAW) {
		n = orig_flags || dflt;
	} else if (n) {
		if (flags) {
			tprint_flags_or();
			print_xlat_val(flags, style);
			n++;
		}

		if (xlat_verbose(style) == XLAT_STYLE_VERBOSE)
			tprint_comment_end();
	} else {
		if (flags) {
			if (xlat_verbose(style) != XLAT_STYLE_VERBOSE)
				print_xlat_val(flags, style);
			tprints_comment(dflt);
		} else {
			if (dflt)
				PRINT_VAL_U(0);
		}
	}

	return n;
}

void
tprint_sockaddr_begin(void)
{
	if (structured_output)
		tprints_object_type("sockaddr");
	else
		tprint_struct_begin();
}

void
tprint_sockaddr_family_begin(void)
{
	if (structured_output) {
		tprints_object_field_begin("family");
		tprint_object_begin();
	} else {
		tprints_field_name("sa_family");
	}
}

void
tprint_sockaddr_family_end(void)
{
	if (structured_output) {
		tprint_object_end();
		tprint_object_field_end();
	}
}

void
tprint_sockaddr_next(void)
{
	if (!structured_output)
		tprint_struct_next();
}

void
tprint_sockaddr_end(void)
{
	if (!structured_output)
		tprint_struct_end();
}

static void
tprints_object_field_hton_u(const char *field, unsigned int val,
			    unsigned int size)
{
	tprints_object_field_begin(field);
	tprint_object_begin();
	PRINT_VAL_U(val);
	tprint_fn_hton_end(size);
	tprint_object_end();
	tprint_object_field_end();
}

void
tprint_sockaddr_in(const char *addr, unsigned int port,
		   void (*print_struct_fields)(const void *), const void *data)
{
	if (structured_output) {
		tprints_object_field_string("addr", addr);
		tprints_object_field_hton_u("port", port, 2);
	} else {
		print_struct_fields(data);
	}
}

void
tprint_sockaddr_in6(const char *addr, unsigned int port,
		    unsigned int flowinfo, bool has_scope_id,
		    unsigned int scope_id, bool scope_id_ifindex,
		    void (*print_struct_fields)(const void *), const void *data)
{
	if (structured_output) {
		tprints_object_field_string("addr", addr);
		tprints_object_field_hton_u("port", port, 2);
		tprints_object_field_hton_u("flowinfo", flowinfo, 4);
		if (has_scope_id && scope_id_ifindex) {
			tprints_object_field_begin("scope_id");
			tprint_object_begin();
			print_ifindex(scope_id);
			tprint_object_end();
			tprint_object_field_end();
		} else if (has_scope_id) {
			tprintf_object_field_string("scope_id", "%u",
						    scope_id);
		}
	} else {
		print_struct_fields(data);
	}
}

void
tprint_sockaddr_nl(unsigned int pad, unsigned int pid, unsigned int groups,
		   void (*print_struct_fields)(const void *), const void *data)
{
	if (structured_output) {
		if (pad)
			tprintf_object_field_string("nl_pad", "%#x", pad);
		tprintf_object_field_string("pid", "%u", pid);
		tprintf_object_field_string("groups", "%#x", groups);
	} else {
		print_struct_fields(data);
	}
}

void
tprint_sun_path_name(void)
{
	if (structured_output)
		tprints_field_name("path");
	else
		tprints_field_name("sun_path");
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

/*
 * Print a value that has `flags' described by `xlat' set on top of it:
 * tprint_flags_with_value_begin prints the flags, the caller prints
 * the value proper, and tprint_flags_with_value_end finishes the job.
 * `raw' is the value with the flags.
 */
void
tprint_flags_with_value_begin(uint64_t raw, uint64_t flags,
			      const struct xlat *xlat, const char *dflt)
{
	if (!structured_output || flags) {
		tprint_flags_groups_begin(raw);
		if (flags) {
			tprint_flags_group_begin(flags);
			printflags64_in(xlat, flags, dflt);
			tprint_flags_group_end();
			tprint_flags_groups_next();
		}
		tprint_flags_group_begin(raw & ~flags);
	}
}

void
tprint_flags_with_value_end(uint64_t flags)
{
	if (!structured_output || flags) {
		tprint_flags_group_end();
		tprint_flags_groups_end();
	}
}

/*
 * Print a syscall number `nr' followed by its `name', if known,
 * prefixed with `nr_prefix', if any.
 */
void
tprint_syscall_nr(unsigned long long nr, const char *name,
		  const char *nr_prefix)
{
	const enum xlat_style style = xlat_verbose(xlat_verbosity);

	if (structured_output) {
		tprints_object_type("syscall");
		if (name)
			tprints_object_field_string("name", name);
		tprintf_object_field_string("scno", "%llu", nr);
	} else {
		if (style != XLAT_STYLE_ABBREV || !nr_prefix)
			PRINT_VAL_U(nr);
		if (style != XLAT_STYLE_RAW && name) {
			if (!nr_prefix || style == XLAT_STYLE_VERBOSE)
				tprint_comment_begin();
			if (nr_prefix)
				tprints_string(nr_prefix);
			tprints_string(name);
			if (!nr_prefix || style == XLAT_STYLE_VERBOSE)
				tprint_comment_end();
		}
	}
}

/*
 * Print a resource limit `lim' followed by its symbolic form `str', if any;
 * `infinity' tells that `str' names an infinite limit.
 */
void
tprint_rlim(unsigned long long lim, const char *str, bool infinity)
{
	const enum xlat_style style = xlat_verbose(xlat_verbosity);

	if (structured_output) {
		if (infinity) {
			tprint_type_const();
			tprintf_object_field_string("raw", "%llu", lim);
			tprints_object_field_string("sym", str);
		} else {
			PRINT_VAL_U(lim);
		}
	} else {
		if (!str || style != XLAT_STYLE_ABBREV)
			PRINT_VAL_U(lim);
		if (str && style == XLAT_STYLE_VERBOSE)
			tprints_comment(str);
		else if (str && style == XLAT_STYLE_ABBREV)
			tprints_string(str);
	}
}

void
tprint_clockid_fd(int clockid, unsigned int fd)
{
	if (structured_output) {
		tprints_object_type("clockid");
		tprintf_object_field_string("raw", "%d", clockid);
		tprints_object_field_string("kind", "fd");
		tprintf_object_field_string("fd", "%u", fd);
	} else {
		PRINT_VAL_D(clockid);
		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_RAW) {
			tprint_comment_begin();
			tprints_fn_begin("FD_TO_CLOCKID");
			PRINT_VAL_U(fd);
			tprint_fn_end();
			tprint_comment_end();
		}
	}
}

/*
 * Start printing a CPU clock id; return false if its clock part
 * is not to be printed.
 */
bool
tprint_clockid_cpu_begin(int clockid, int pid, bool per_thread)
{
	bool print_clock = true;

	if (structured_output) {
		tprints_object_type("clockid");
		tprintf_object_field_string("raw", "%d", clockid);
		tprints_object_field_string("kind", "cpu");
		tprints_object_field_begin("pid");
		tprint_object_begin();
		tprints_object_type(per_thread ? "tid" : "tgid");
		tprintf_object_field_string("raw", "%d", pid);
		tprint_object_end();
		tprint_object_field_end();
		tprints_object_field_bool("per_thread", per_thread);
		tprints_object_field_begin("cpu_clock");
		tprint_object_begin();
	} else {
		PRINT_VAL_D(clockid);
		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_RAW) {
			print_clock = false;
		} else {
			tprint_comment_begin();
			tprints_fn_begin(per_thread ? "MAKE_THREAD_CPUCLOCK"
						    : "MAKE_PROCESS_CPUCLOCK");
			PRINT_VAL_D(pid);
			tprint_fn_next();
		}
	}

	return print_clock;
}

void
tprint_clockid_cpu_end(void)
{
	if (structured_output) {
		tprint_object_end();
		tprint_object_field_end();
	} else {
		tprint_fn_end();
		tprint_comment_end();
	}
}

void
tprint_clockid_aux(int clockid, unsigned int index)
{
	if (structured_output) {
		tprints_object_type("clockid");
		tprintf_object_field_string("raw", "%#x", clockid);
		tprints_object_field_string("kind", "aux");
		tprintf_object_field_string("aux_index", "%u", index);
	} else {
		const enum xlat_style style = xlat_verbose(xlat_verbosity);

		if (style != XLAT_STYLE_ABBREV)
			PRINT_VAL_X(clockid);
		if (style != XLAT_STYLE_RAW) {
			if (style == XLAT_STYLE_VERBOSE)
				tprint_comment_begin();
			print_xlat_ex(clockid - index, "CLOCK_AUX",
				      XLAT_STYLE_ABBREV);
			tprint_plus();
			PRINT_VAL_U(index);
			if (style == XLAT_STYLE_VERBOSE)
				tprint_comment_end();
		}
	}
}

/*
 * Start printing a quotactl command; return false if its parts
 * are not to be printed.
 */
bool
tprint_qcmd_begin(unsigned int qcmd)
{
	const enum xlat_style style = xlat_verbose(xlat_verbosity);

	if (structured_output) {
		tprints_object_type("qcmd_op");
		tprintf_object_field_string("raw", "%#x", qcmd);
		tprints_object_field_begin("cmd");
		tprint_object_begin();
	} else {
		if (style != XLAT_STYLE_ABBREV)
			PRINT_VAL_U(qcmd);
		if (style != XLAT_STYLE_RAW) {
			if (style == XLAT_STYLE_VERBOSE)
				tprint_comment_begin();
			tprints_fn_begin("QCMD");
		}
	}

	return structured_output || style != XLAT_STYLE_RAW;
}

void
tprint_qcmd_next(void)
{
	if (structured_output) {
		tprint_object_end();
		tprint_object_field_end();
		tprints_object_field_begin("quota_type");
		tprint_object_begin();
	} else {
		tprint_fn_next();
	}
}

void
tprint_qcmd_end(void)
{
	if (structured_output) {
		tprint_object_end();
		tprint_object_field_end();
	} else {
		tprint_fn_end();

		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
			tprint_comment_end();
	}
}

/*
 * Print the I/O priority value `ioprio' whose decoded form is `str';
 * the raw value is printed as a signed decimal number if `signed_raw',
 * as a hexadecimal number otherwise.  In structured output mode,
 * the priority class `class' is decoded using `class_xlat'.
 */
void
tprint_ioprio(unsigned int ioprio, bool signed_raw, const char *str,
	      const struct xlat *class_xlat, unsigned int class,
	      unsigned int level)
{
	if (structured_output) {
		tprints_object_type("ioprio");
		if (signed_raw) {
			tprintf_object_field_string("raw", "%d",
						    (int) ioprio);
		} else {
			tprintf_object_field_string("raw", "%#x", ioprio);
		}
		tprints_object_field_begin("class");
		tprint_object_begin();
		printxval_ex(class_xlat, class, "IOPRIO_CLASS_???",
			     XLAT_STYLE_DEFAULT);
		tprint_object_end();
		tprint_object_field_end();
		tprintf_object_field_string("level", "%u", level);
	} else {
		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_ABBREV) {
			if (signed_raw)
				PRINT_VAL_D((int) ioprio);
			else
				PRINT_VAL_X(ioprio);
		}

		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_RAW) {
			if (xlat_verbose(xlat_verbosity)
			    == XLAT_STYLE_VERBOSE)
				tprints_comment(str);
			else
				tprints_string(str);
		}
	}
}

void
tprint_port_range(unsigned int ports)
{
	const unsigned short lo = ports & 0xffff;
	const unsigned short hi = ports >> 16;
	const bool well_formed = ports && (!lo || !hi || lo <= hi);

	if (structured_output) {
		tprints_object_type("port_range");
		tprintf_object_field_string("raw", "%#x", ports);
		if (well_formed) {
			tprintf_object_field_string("lo", "%hu", lo);
			tprintf_object_field_string("hi", "%hu", hi);
		}
	} else {
		PRINT_VAL_X(ports);
		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_RAW
		    && well_formed)
			tprintf_comment("%.0hu..%.0hu", lo, hi);
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
tprint_numeric_mode(unsigned long long mode)
{
	if (structured_output) {
		tprints_object_type("mode");
		tprintf_object_field_string("raw", "%#03llo", mode);
	} else {
		PRINT_VAL_03O(mode);
	}
}

void
tprint_symbolic_mode(unsigned int mode, const char *ifmt)
{
	static const struct {
		unsigned int bit;
		const char *name;
	} special_bits[] = {
		{ S_ISUID, "S_ISUID" },
		{ S_ISGID, "S_ISGID" },
		{ S_ISVTX, "S_ISVTX" },
	};

	if (structured_output) {
		tprints_object_type("mode");
		tprintf_object_field_string("raw", "%#03o", mode);
		if (ifmt) {
			if (ifmt[0]) {
				tprints_object_field_begin("filetype");
				tprint_object_begin();
				tprints_object_type("const");
				tprintf_object_field_string("raw", "%#o",
							    mode & S_IFMT);
				tprints_object_field_string("sym", ifmt);
				tprint_object_end();
				tprint_object_field_end();
			}
			tprints_object_field_begin("special");
			tprint_list_begin();
			for (size_t i = 0; i < ARRAY_SIZE(special_bits); ++i) {
				if (!(mode & special_bits[i].bit))
					continue;
				tprint_list_element_begin();
				tprints_string_value(special_bits[i].name);
				tprint_list_element_end();
			}
			tprint_list_end();
			tprint_object_field_end();
			tprintf_object_field_string("perm", "%#03o",
				mode & ~(S_IFMT|S_ISUID|S_ISGID|S_ISVTX));
		}
	} else {
		if (!ifmt || xlat_verbose(xlat_verbosity) != XLAT_STYLE_ABBREV)
			PRINT_VAL_03O(mode);

		if (!ifmt || xlat_verbose(xlat_verbosity) == XLAT_STYLE_RAW)
			return;

		(xlat_verbose(xlat_verbosity) == XLAT_STYLE_ABBREV
			? tprintf_string : tprintf_comment)("%s%s%s%s%s%#03o",
				ifmt, ifmt[0] ? "|" : "",
				(mode & S_ISUID) ? "S_ISUID|" : "",
				(mode & S_ISGID) ? "S_ISGID|" : "",
				(mode & S_ISVTX) ? "S_ISVTX|" : "",
				mode & ~(S_IFMT|S_ISUID|S_ISGID|S_ISVTX));
	}
}

void
tprint_dev_t(unsigned long long dev, unsigned int dev_major,
	     unsigned int dev_minor)
{
	if (structured_output) {
		tprints_object_type("dev_t");
		tprintf_object_field_string("raw", "%#llx", dev);
		tprintf_object_field_string("major", "%u", dev_major);
		tprintf_object_field_string("minor", "%u", dev_minor);
	} else {
		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_ABBREV)
			PRINT_VAL_X(dev);

		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_RAW) {
			if (xlat_verbose(xlat_verbosity)
			    == XLAT_STYLE_VERBOSE)
				tprint_comment_begin();

			tprints_fn_begin("makedev");
			PRINT_VAL_X(dev_major);

			tprint_fn_next();
			PRINT_VAL_X(dev_minor);
			tprint_fn_end();

			if (xlat_verbose(xlat_verbosity)
			    == XLAT_STYLE_VERBOSE)
				tprint_comment_end();
		}
	}
}

void
tprint_var_count(unsigned long long addr, unsigned int count,
		 bool unterminated)
{
	if (structured_output) {
		tprints_object_type("addr_count");
		tprintf_object_field_string("raw", "%#llx", addr);
		tprintf_object_field_string("count", "%u", count);
		tprints_object_field_string("unit", "vars");
		if (unterminated)
			tprints_object_field_bool("unterminated", true);
	} else {
		printaddr64(addr);
		tprintf_comment("%u var%s%s", count, count == 1 ? "" : "s",
				unterminated ? ", unterminated" : "");
	}
}

void
tprints_uuid(const char *str)
{
	if (structured_output) {
		tprints_object_type("uuid");
		tprints_object_field_string("value", str);
	} else {
		tprints_string(str);
	}
}

static void
tprint_time_fields(const char *type, long long sec, const char *frac_name,
		   const char *frac)
{
	tprints_object_type(type);
	tprints_object_field_begin("fields");
	tprint_object_begin();
	tprintf_object_field_string("tv_sec", "%lld", sec);
	tprints_object_field_string(frac_name, frac);
	tprint_object_end();
	tprint_object_field_end();
}

void
tprint_timespec(long long sec, unsigned long long nsec)
{
	if (structured_output) {
		char frac[sizeof(nsec) * 3];

		xsprintf(frac, "%llu", nsec);
		tprint_time_fields("timespec_t", sec, "tv_nsec", frac);
	} else {
		tprint_struct_begin();
		tprints_field_name("tv_sec");
		PRINT_VAL_D(sec);
		tprint_struct_next();
		tprints_field_name("tv_nsec");
		PRINT_VAL_U(nsec);
		tprint_struct_end();
	}
}

void
tprint_timespec_signed(long long sec, long long nsec)
{
	if (structured_output) {
		char frac[sizeof(nsec) * 3];

		xsprintf(frac, "%lld", nsec);
		tprint_time_fields("timespec_t", sec, "tv_nsec", frac);
	} else {
		tprint_struct_begin();
		tprints_field_name("tv_sec");
		PRINT_VAL_D(sec);
		tprint_struct_next();
		tprints_field_name("tv_nsec");
		PRINT_VAL_D(nsec);
		tprint_struct_end();
	}
}

void
tprint_timespec_sym(long long sec, unsigned long long nsec, const char *sym)
{
	if (structured_output) {
		tprint_timespec(sec, nsec);
		tprints_object_field_string("sym", sym);
	} else {
		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_ABBREV)
			tprint_timespec(sec, nsec);
		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
			tprints_comment(sym);
		else if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_ABBREV)
			tprints_string(sym);
	}
}

void
tprint_timeval(long long sec, unsigned long long usec)
{
	if (structured_output) {
		char frac[sizeof(usec) * 3];

		xsprintf(frac, "%llu", usec);
		tprint_time_fields("timeval_t", sec, "tv_usec", frac);
	} else {
		tprint_struct_begin();
		tprints_field_name("tv_sec");
		PRINT_VAL_D(sec);
		tprint_struct_next();
		tprints_field_name("tv_usec");
		PRINT_VAL_U(usec);
		tprint_struct_end();
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
tprint_dirent_count_begin(unsigned long long addr)
{
	if (structured_output) {
		tprints_object_type("addr_count");
		tprintf_object_field_string("raw", "%#llx", addr);
	} else {
		printaddr64(addr);
	}
}

void
tprint_dirent_count_end(unsigned int count, bool truncated)
{
	if (structured_output) {
		tprintf_object_field_string("count", "%u", count);
		tprints_object_field_string("unit", "entries");
		if (truncated)
			tprints_object_field_bool("truncated", true);
	} else {
		tprintf_comment("%u%s entries", count, truncated ? "+" : "");
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
tprints_timestamp_comment(const char *str)
{
	if (!structured_output)
		tprints_comment(str);
}

void
tprint_time_t(long long sec, const char *str)
{
	if (structured_output) {
		tprints_object_type("time_t");
		tprintf_object_field_string("raw", "%lld", sec);
	} else {
		PRINT_VAL_D(sec);
		tprints_comment(str);
	}
}

void
tprint_kernel_version(unsigned long version)
{
	const unsigned long ver_major = version >> 16;
	const unsigned long ver_minor = (version >> 8) & 0xFF;
	const unsigned long ver_patch = version & 0xFF;

	if (structured_output) {
		tprints_object_type("kernel_version");
		tprintf_object_field_string("raw", "%lu", version);
		tprintf_object_field_string("major", "%lu", ver_major);
		tprintf_object_field_string("minor", "%lu", ver_minor);
		tprintf_object_field_string("patch", "%lu", ver_patch);
	} else {
		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_ABBREV)
			PRINT_VAL_X(version);

		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_RAW) {
			if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
				tprint_comment_begin();

			tprints_fn_begin("KERNEL_VERSION");
			PRINT_VAL_U(ver_major);

			tprint_fn_next();
			PRINT_VAL_U(ver_minor);

			tprint_fn_next();
			PRINT_VAL_U(ver_patch);
			tprint_fn_end();

			if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
				tprint_comment_end();
		}
	}
}

void
tprint_ticks(unsigned long long val, long freq, unsigned int precision)
{
	if (structured_output) {
		tprints_object_type("ticks");
		tprintf_object_field_string("raw", "%llu", val);
		tprintf_object_field_string("hz", "%ld", freq);
		tprintf_object_field_string("precision", "%u", precision);
	} else {
		PRINT_VAL_U(val);
		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_RAW
		    && freq > 0 && val > 0)
			print_ticks_comment(val, freq, precision, false);
	}
}

void
tprint_ticks_d(long long val, long freq, unsigned int precision)
{
	if (structured_output) {
		tprints_object_type("ticks");
		tprintf_object_field_string("raw", "%lld", val);
		tprintf_object_field_string("hz", "%ld", freq);
		tprintf_object_field_string("precision", "%u", precision);
	} else {
		PRINT_VAL_D(val);
		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_RAW
		    && freq > 1 && val != 0) {
			const unsigned long long magnitude =
				val < 0 ? -(unsigned long long) val
					: (unsigned long long) val;

			print_ticks_comment(magnitude, freq, precision,
					    val < 0);
		}
	}
}

void
tprint_err(long long err, bool negated, const char *name)
{
	if (structured_output) {
		tprints_object_type(name ? "errno"
					 : negated ? "decimal" : "unsigned");
		tprintf_object_field_string("raw", negated ? "%lld" : "%llu",
					    err);
		if (name)
			tprints_object_field_string("sym", name);
	} else {
		if (!name || xlat_verbose(xlat_verbosity) != XLAT_STYLE_ABBREV)
			tprintf_string(negated ? "%lld" : "%llu", err);
		if (!name || xlat_verbose(xlat_verbosity) == XLAT_STYLE_RAW)
			return;
		(xlat_verbose(xlat_verbosity) == XLAT_STYLE_ABBREV
			? tprintf_string : tprintf_comment)("%s%s",
							    negated ? "-" : "",
							    name);
	}
}

void
tprint_ifindex(unsigned int ifindex, const char *name)
{
	if (structured_output) {
		tprints_object_type("ifindex");
		tprintf_object_field_string("raw", "%u", ifindex);
		if (name)
			tprints_object_field_string("ifname", name);
	} else {
		char *str = name ? xasprintf("if_nametoindex(\"%s\")", name)
				 : NULL;

		print_xlat_ex(ifindex, str, XLAT_STYLE_FMT_U);
		free(str);
	}
}

void
tprint_ip_addr(const char *var_name, bool v6, const void *addr,
	       unsigned int len, const char *str)
{
	if (structured_output) {
		if (var_name)
			tprints_field_name(var_name);

		tprints_object_type("ip_addr");
		tprints_object_field_begin("family");
		tprint_object_begin();
		tprints_object_type("const");
		tprintf_object_field_string("raw", "%#x",
					    v6 ? AF_INET6 : AF_INET);
		tprints_object_field_string("sym",
					    v6 ? "AF_INET6" : "AF_INET");
		tprint_object_end();
		tprint_object_field_end();
		tprints_object_field_string("addr", str);
	} else if (!v6) {
		if (var_name)
			tprints_field_name(var_name);

		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_ABBREV)
			print_quoted_string(addr, len, QUOTE_FORCE_HEX);

		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_RAW)
			return;

		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
			tprint_comment_begin();

		tprints_fn_begin("inet_addr");
		print_quoted_string(str, strlen(str) + 1, QUOTE_0_TERMINATED);
		tprint_fn_end();

		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
			tprint_comment_end();
	} else {
		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_ABBREV) {
			if (var_name)
				tprints_field_name(var_name);
			print_quoted_string(addr, len, QUOTE_FORCE_HEX);
		}

		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_RAW)
			return;

		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
			tprint_comment_begin();

		tprints_fn_begin("inet_pton");
		print_xlat_ex(AF_INET6, "AF_INET6", XLAT_STYLE_ABBREV);

		tprint_fn_next();
		print_quoted_string(str, strlen(str) + 1, QUOTE_0_TERMINATED);

		if (var_name &&
		    (xlat_verbose(xlat_verbosity) == XLAT_STYLE_ABBREV)) {
			tprint_fn_next();
			tprintf_string("&%s", var_name);
		}
		tprint_fn_end();

		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
			tprint_comment_end();
	}
}

void
tprint_mac_addr(const void *addr, unsigned int size, const char *str)
{
	if (structured_output) {
		if (str) {
			tprints_object_type("mac");
			tprints_object_field_string("value", str);
		} else {
			tprints_object_type("bytes");
			print_quoted_string(addr, size, QUOTE_FORCE_HEX);
		}
	} else {
		if (!str || xlat_verbose(xlat_verbosity) != XLAT_STYLE_ABBREV)
			print_quoted_string(addr, size, QUOTE_FORCE_HEX);
		if (!str || xlat_verbose(xlat_verbosity) == XLAT_STYLE_RAW)
			return;
		(xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE
			? tprints_comment : tprints_string)(str);
	}
}

static const char *
hton_fn_name(unsigned int size)
{
	return size == 2 ? "htons" : size == 4 ? "htonl" : "htobe64";
}

void
tprint_fn_hton_begin(unsigned int size)
{
	if (structured_output)
		return;

	tprints_fn_begin(hton_fn_name(size));
}

void
tprint_fn_hton_end(unsigned int size)
{
	if (structured_output) {
		tprints_object_field_string("byteorder", "big");
		tprints_object_field_begin("size");
		STRACE_PRINTF("%u", size);
		tprint_object_field_end();
	} else {
		tprint_fn_end();
	}
}

void
tprint_ax25_addr(const void *call, unsigned int size, enum xlat_style style,
		 const char *str)
{
	if (structured_output) {
		tprints_object_type("ax25_address");
		print_quoted_string(call, size, QUOTE_FORCE_HEX);
	} else {
		if (style != XLAT_STYLE_ABBREV) {
			tprint_struct_begin();
			tprints_field_name("ax25_call");
			print_quoted_string(call, size, QUOTE_FORCE_HEX);
			tprint_struct_end();
		}

		if (style == XLAT_STYLE_VERBOSE)
			tprints_comment(str);
		else if (style == XLAT_STYLE_ABBREV)
			tprints_string(str);
	}
}

void
tprint_net_port(const void *raw, unsigned int port)
{
	if (structured_output) {
		PRINT_VAL_U(port);
		tprint_fn_hton_end(2);
	} else {
		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_ABBREV)
			print_quoted_string(raw, 2, QUOTE_FORCE_HEX);

		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_RAW)
			return;

		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
			tprint_comment_begin();

		tprint_fn_hton_begin(2);
		PRINT_VAL_U(port);
		tprint_fn_hton_end(2);

		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
			tprint_comment_end();
	}
}

void
tprint_addr(unsigned long long addr)
{
	PRINT_VAL_ADDR(addr);
}

void
tprint_secontext(const char *ctx, unsigned int ctx_len,
		 const char *expected, unsigned int expected_len,
		 unsigned int style)
{
	if (structured_output) {
		tprints_object_field_begin("secontext");
		tprint_object_begin();
		tprints_object_field_bytes("context", ctx, ctx_len);
		if (expected)
			tprints_object_field_bytes("expected", expected,
						   expected_len);
		tprint_object_end();
		tprint_object_field_end();
	} else {
		tprint_space();
		tprint_attribute_begin();
		print_quoted_string_ex(ctx, ctx_len, style, "[]!");
		if (expected) {
			tprints_string("!!");
			print_quoted_string_ex(expected, expected_len, style,
					       "[]!");
		}
		tprint_attribute_end();
	}
}

void
tprint_secontext_pid(const char *ctx, unsigned int ctx_len,
		     unsigned int style)
{
	tprint_attribute_begin();
	print_quoted_string_ex(ctx, ctx_len, style, "[]!");
	tprint_attribute_end();
	tprint_space();
}

void
tprint_net_xval(const void *raw, unsigned int size, const struct xlat *xlat,
		unsigned long long val, const char *dflt)
{
	if (structured_output) {
		printxval_ex(xlat, val, dflt, XLAT_STYLE_ABBREV);
		tprint_fn_hton_end(size);
	} else {
		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_ABBREV)
			print_quoted_string(raw, size, QUOTE_FORCE_HEX);

		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_RAW)
			return;

		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
			tprint_comment_begin();

		tprint_fn_hton_begin(size);
		printxval_ex(xlat, val, dflt, XLAT_STYLE_ABBREV);
		tprint_fn_hton_end(size);

		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
			tprint_comment_end();
	}
}

void
tprint_hton_xlat(unsigned long long stored, unsigned long long val,
		 const char *sym, unsigned int size)
{
	if (structured_output) {
		print_xlat_ex(val, sym, XLAT_STYLE_DEFAULT);
		tprint_fn_hton_end(size);
	} else {
		char *str = xasprintf("%s(%s)", hton_fn_name(size), sym);

		print_xlat_ex(stored, str, XLAT_STYLE_DEFAULT);
		free(str);
	}
}

void
tprint_wait_status(int status, const struct xlat *events)
{
	/*
	 * Here is a tricky presentation problem.  This solution
	 * is still not entirely satisfactory but since there
	 * are no wait status constructors it will have to do.
	 */
	tprint_indirect_begin();
	if (structured_output) {
		tprints_object_type("wait_status");
		tprintf_object_field_string("raw", "%#x", status);
		if (WIFSTOPPED(status)) {
			int sig = WSTOPSIG(status);
			tprints_object_field_string("status", "stopped");
			tprints_object_field_begin("signal");
			tprint_object_begin();
			tprint_signal(sig & 0x7f, signame(sig & 0x7f));
			tprint_object_end();
			tprint_object_field_end();
			if (sig & 0x80)
				tprints_object_field_bool("sig_0x80", true);
			status &= ~W_STOPCODE(sig);
		} else if (WIFSIGNALED(status)) {
			tprints_object_field_string("status", "signaled");
			tprints_object_field_begin("signal");
			tprint_object_begin();
			tprint_signal(WTERMSIG(status),
				      signame(WTERMSIG(status)));
			tprint_object_end();
			tprint_object_field_end();
			if (WCOREDUMP(status))
				tprints_object_field_bool("coredumped", true);
			status &= ~(W_EXITCODE(0, WTERMSIG(status))
				    | WCOREFLAG);
		} else if (WIFEXITED(status)) {
			tprints_object_field_string("status", "exited");
			tprintf_object_field_string("exit_code", "%d",
						    WEXITSTATUS(status));
			status &= ~W_EXITCODE(WEXITSTATUS(status), 0);
		}
#ifdef WIFCONTINUED
		else if (WIFCONTINUED(status)) {
			tprints_object_field_string("status", "continued");
			status &= ~W_CONTINUED;
		}
#endif
		else {
			tprints_object_field_string("status", "unknown");
			status = 0;
		}

		unsigned int event = (unsigned int) status >> 16;
		if (event) {
			tprints_object_field_begin("ptrace_event");
			tprint_object_begin();
			printxval(events, event, "PTRACE_EVENT_???");
			tprint_object_end();
			tprint_object_field_end();
			status &= 0xffff;
		}
		if (status)
			tprintf_object_field_string("remainder", "%#x",
						    status);
	} else {
		tprint_flags_begin();
		if (WIFSTOPPED(status)) {
			int sig = WSTOPSIG(status);
			tprintf_string("{WIFSTOPPED(s) && WSTOPSIG(s) == %s%s}",
				       sprintsigname(sig & 0x7f),
				       sig & 0x80 ? " | 0x80" : "");
			status &= ~W_STOPCODE(sig);
		} else if (WIFSIGNALED(status)) {
			tprintf_string("{WIFSIGNALED(s)"
				       " && WTERMSIG(s) == %s%s}",
				       sprintsigname(WTERMSIG(status)),
				       WCOREDUMP(status)
				       ? " && WCOREDUMP(s)" : "");
			status &= ~(W_EXITCODE(0, WTERMSIG(status))
				    | WCOREFLAG);
		} else if (WIFEXITED(status)) {
			tprintf_string("{WIFEXITED(s) && WEXITSTATUS(s) == %d}",
				       WEXITSTATUS(status));
			status &= ~W_EXITCODE(WEXITSTATUS(status), 0);
		}
#ifdef WIFCONTINUED
		else if (WIFCONTINUED(status)) {
			tprints_string("{WIFCONTINUED(s)}");
			status &= ~W_CONTINUED;
		}
#endif
		else {
			PRINT_VAL_X(status);
			status = 0;
		}

		if (status) {
			unsigned int event = (unsigned int) status >> 16;
			if (event) {
				tprint_flags_or();
				tprint_shift_begin();
				printxval(events, event, "PTRACE_EVENT_???");
				tprint_shift();
				PRINT_VAL_U(16);
				tprint_shift_end();
				status &= 0xffff;
			}
			if (status) {
				tprint_flags_or();
				PRINT_VAL_X(status);
			}
		}
		tprint_flags_end();
	}
	tprint_indirect_end();
}

void
tprint_signal(int nr, const char *name)
{
	if (structured_output) {
		tprints_object_type("signal");
		tprintf_object_field_string("raw", "%d", nr);
		if (name)
			tprints_object_field_string("sym", name);
	} else {
		if (!name
		    || xlat_verbose(xlat_verbosity) != XLAT_STYLE_ABBREV)
			PRINT_VAL_D(nr);

		if (name && xlat_verbose(xlat_verbosity) != XLAT_STYLE_RAW) {
			if (xlat_verbose(xlat_verbosity)
			    == XLAT_STYLE_VERBOSE)
				tprints_comment(name);
			else
				tprints_string(name);
		}
	}
}

void
tprint_perf_precise_ip(unsigned int val, const char *desc, const char *skid)
{
	if (structured_output) {
		tprints_object_type("perf_precise_ip");
		tprintf_object_field_string("raw", "%u", val);
		tprints_object_field_string("skid", skid);
	} else {
		PRINT_VAL_U(val);
		tprints_comment(desc);
	}
}

void
tprint_reserved_bits(unsigned long long val, unsigned int from,
		     unsigned int to)
{
	if (structured_output) {
		tprints_object_type("reserved_bits");
		tprintf_object_field_string("raw", "%#llx", val);
		tprintf_object_field_string("first_bit", "%u", MIN(from, to));
		tprintf_object_field_string("last_bit", "%u", MAX(from, to));
	} else {
		PRINT_VAL_X(val);
		tprintf_comment("Bits %u..%u", from, to);
	}
}

void
tprint_sigmask_begin(const uint32_t *mask, unsigned int nwords, bool inverted)
{
	if (structured_output) {
		char raw[sizeof("0x") + 2 * sizeof(uint32_t) * 8];
		char *p = raw;

		while (nwords > 0 && !mask[nwords - 1])
			--nwords;
		if (nwords > 8)
			nwords = 8;

		tprints_object_type("bitset");
		if (nwords) {
			p = xappendstr(raw, p, "%#x", mask[nwords - 1]);
			for (unsigned int i = nwords - 1; i > 0; --i)
				p = xappendstr(raw, p, "%08x", mask[i - 1]);
			tprints_object_field_string("raw", raw);
		} else {
			tprints_object_field_string("raw", "0");
		}
		if (inverted)
			tprints_object_field_bool("inverted", true);
		tprints_object_field_begin("elems");
		tprint_list_begin();
	} else {
		if (inverted)
			STRACE_PRINTS("~");
		STRACE_PRINTS("[");
	}
}

void
tprint_sigmask_next(void)
{
	if (!structured_output)
		STRACE_PRINTS(" ");
}

void
tprint_sigmask_end(void)
{
	if (structured_output) {
		tprint_list_end();
		tprint_object_field_end();
	} else {
		STRACE_PRINTS("]");
	}
}

void
tprint_sigmask_member(unsigned int sig, const char *name)
{
	if (structured_output) {
		tprint_list_element_begin();
		tprint_object_begin();
		tprint_signal(sig, name);
		tprint_object_end();
		tprint_list_element_end();
	} else {
		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_ABBREV)
			STRACE_PRINTF("%u", sig);
		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
			STRACE_PRINTS(" /* ");
		if (xlat_verbose(xlat_verbosity) != XLAT_STYLE_RAW) {
			if (name)
				STRACE_PRINTS(name + 3);
			else if (xlat_verbose(xlat_verbosity)
				 != XLAT_STYLE_ABBREV)
				STRACE_PRINTF("%u", sig);
		}
		if (xlat_verbose(xlat_verbosity) == XLAT_STYLE_VERBOSE)
			STRACE_PRINTS(" */");
	}
}

void
tprint_unused_bits(unsigned long long val, unsigned int first,
		   unsigned int last)
{
	if (structured_output) {
		tprints_object_type("reserved_bits");
		tprintf_object_field_string("raw", "%#llx", val);
		tprintf_object_field_string("first_bit", "%u", first);
		tprintf_object_field_string("last_bit", "%u", last);
	} else {
		PRINT_VAL_X(val);
		tprintf_comment("bits %u..%u", first, last);
	}
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

void
tprint_io_dump_mmsg_begin(void)
{
}

void
tprint_io_dump_mmsg_vector_begin(unsigned long long iovlen, unsigned int index)
{
	tprintf_string(" = %llu buffers in vector %u\n", iovlen, index);
}

void
tprint_io_dump_mmsg_vector_end(void)
{
}

void
tprint_io_dump_mmsg_end(void)
{
}
