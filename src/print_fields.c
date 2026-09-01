/*
 * Copyright (c) 2016-2017 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2017-2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"
#include "print_fields.h"

#if ENABLE_STRUCTURED_OUTPUT
struct structured_output_data *structured_output = NULL;
#endif

void
json_print_quoted_string_begin(void)
{
	if (!structured_output)
		return;

	STRACE_PRINTS("\"");
}

void
json_print_quoted_string_end(void)
{
	if (!structured_output)
		return;

	STRACE_PRINTS("\"");
}

void
tprints_string_value(const char *str)
{
	if (structured_output) {
		json_print_quoted_string_begin();
		print_quoted_string_ex(str, strlen(str),
				       QUOTE_OMIT_LEADING_TRAILING_QUOTES,
				       NULL);
		json_print_quoted_string_end();
	} else {
		STRACE_PRINTS(str);
	}
}

ATTRIBUTE_FORMAT((printf, 1, 0))
void
tprintv_string_value(const char *fmt, va_list args)
{
	json_print_quoted_string_begin();
	STRACE_PRINTV(fmt, args);
	json_print_quoted_string_end();
}

ATTRIBUTE_FORMAT((printf, 1, 2))
void
tprintf_string_value(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	tprintv_string_value(fmt, args);
	va_end(args);
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
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(", ");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_struct_end(void)
{
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
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(", ");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_union_end(void)
{
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
tprint_array_index_begin(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("[");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_array_index_equal(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("]=");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprints_arg_begin(const char *name)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_SYSCALL);
	STRACE_PRINTF("%s", name);
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
	STRACE_PRINTF("(%s)", str);
}

void
tprint_sysret_end(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_RESET);
}
