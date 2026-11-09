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

struct structured_output_data *structured_output = NULL;

static const char *
json_container_type_name(enum json_container_type type)
{
	switch (type) {
	case JSON_CONTAINER_OBJECT:
		return "object";
	case JSON_CONTAINER_ARRAY:
		return "array";
	default:
		return "unknown";
	}
}

static void
json_stack_push(enum json_container_type type)
{
	if (!structured_output)
		return;

	if (structured_output->state.depth >= JSON_STACK_MAX)
		error_func_msg_and_die("structured output stack overflow"
				       " (limit=%u)", JSON_STACK_MAX);

	unsigned int depth = structured_output->state.depth;
	structured_output->state.stack[depth] =
		(struct json_stack_frame) { .type = type };
	structured_output->state.depth++;
}

static void
json_stack_pop(void)
{
	if (!structured_output)
		return;

	if (structured_output->state.depth == 0)
		error_func_msg_and_die("structured output stack underflow");

	structured_output->state.depth--;
}

static enum json_container_type
json_stack_top_type(void)
{
	if (!structured_output)
		return JSON_CONTAINER_OBJECT;

	if (structured_output->state.depth == 0)
		return JSON_CONTAINER_OBJECT;

	const struct json_stack *state = &structured_output->state;
	return state->stack[state->depth - 1].type;
}

static void
json_stack_set_needs_sep(enum json_container_type type)
{
	struct json_stack *state = &structured_output->state;

	if (state->depth == 0)
		error_func_msg_and_die("tried to enable %s separator"
				       " on an empty stack",
				       json_container_type_name(type));

	enum json_container_type top = json_stack_top_type();
	if (top != type)
		error_func_msg_and_die("tried to enable %s separator"
				       " but the context is %s",
				       json_container_type_name(type),
				       json_container_type_name(top));

	state->stack[state->depth - 1].needs_sep = true;
}

static bool
json_stack_needs_sep(enum json_container_type type)
{
	const struct json_stack *state = &structured_output->state;

	if (state->depth == 0)
		error_func_msg_and_die("tried to query %s separator"
				       " on an empty stack",
				       json_container_type_name(type));

	enum json_container_type top = json_stack_top_type();
	if (top != type)
		error_func_msg_and_die("tried to print %s separator"
				       " but the context is %s",
				       json_container_type_name(type),
				       json_container_type_name(top));

	return state->stack[state->depth - 1].needs_sep;
}

static void
json_stack_reset(void)
{
	if (!structured_output)
		return;

	structured_output->state.depth = 0;
}

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
	if (structured_output)
		json_print_quoted_string_begin();
	STRACE_PRINTS(str);
	if (structured_output)
		json_print_quoted_string_end();
}

void
json_prints_string_value(const char *str)
{
	if (!structured_output)
		return;

	tprints_string_value(str);
}

void
json_printf_string_value(const char *fmt, ...)
{
	if (!structured_output)
		return;

	va_list args;
	va_start(args, fmt);
	tprintv_string_value(fmt, args);
	va_end(args);
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
tprint_object_begin(void)
{
	if (structured_output) {
		STRACE_PRINTS(JSON_OBJ_BEGIN);
		json_stack_push(JSON_CONTAINER_OBJECT);
	}
}

void
tprints_object_field_begin(const char *field)
{
	if (!structured_output)
		return;

	if (structured_output->state.depth == 0
	    || json_stack_top_type() != JSON_CONTAINER_OBJECT)
		error_func_msg_and_die("structured output field \"%s\" outside"
				       " of an object", field);

	if (json_stack_needs_sep(JSON_CONTAINER_OBJECT))
		STRACE_PRINTS(JSON_SEP);

	STRACE_PRINTF("\"%s\"", field);
	STRACE_PRINTS(JSON_FIELD_SEP);
}

void
json_prints_object_field_begin(const char *field)
{
	if (!structured_output)
		return;

	tprints_object_field_begin(field);
}

void
tprint_object_field_end(void)
{
	if (structured_output)
		json_stack_set_needs_sep(JSON_CONTAINER_OBJECT);
}

void
json_print_object_field_end(void)
{
	if (structured_output)
		tprint_object_field_end();
}

void
tprints_object_field_string(const char *field, const char *s)
{
	tprints_object_field_begin(field);
	tprints_string_value(s);
	tprint_object_field_end();
}

void
json_prints_object_field_string(const char *field, const char *s)
{
	if (!structured_output)
		return;

	tprints_object_field_string(field, s);
}

void
json_print_object_field_bool(const char *field, bool val)
{
	if (!structured_output)
		return;

	tprints_object_field_begin(field);
	STRACE_PRINTS(val ? JSON_TRUE : JSON_FALSE);
	tprint_object_field_end();
}

void
json_print_object_field_null(const char *field)
{
	if (!structured_output)
		return;

	tprints_object_field_begin(field);
	STRACE_PRINTS(JSON_NULL);
	tprint_object_field_end();
}

void
tprints_object_type(const char *type)
{
	if (structured_output)
		tprints_object_field_string("type", type);
}

void
tprint_type_const(void)
{
	if (structured_output)
		tprints_object_field_string("type", "const");
}

ATTRIBUTE_FORMAT((printf, 2, 3))
void
tprintf_object_field_string(const char *field, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if (structured_output) {
		tprints_object_field_begin(field);
		tprintv_string_value(fmt, args);
		tprint_object_field_end();
	} else {
		STRACE_PRINTV(fmt, args);
	}
	va_end(args);
}

void
tprint_object_end(void)
{
	if (structured_output) {
		if (structured_output->state.depth == 0
		    || json_stack_top_type() != JSON_CONTAINER_OBJECT)
			error_func_msg_and_die("structured output object"
					       " end outside of an object");
		json_stack_pop();
		STRACE_PRINTS(JSON_OBJ_END);
	}
}

void
tprint_struct_begin(void)
{
	if (structured_output) {
		tprints_object_field_begin("type");
		tprintf_string_value("%s", "struct");
		tprint_object_field_end();
		tprints_object_field_begin("fields");
		tprint_object_begin();
		structured_output->state.stack[structured_output->state.depth - 1]
			.struct_fields = true;
	} else {
		STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
		STRACE_PRINTS("{");
		STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
	}
}

static void
json_close_struct_field(void)
{
	const struct json_stack *state = &structured_output->state;

	if (state->depth && !state->stack[state->depth - 1].struct_fields)
		tprint_object_end();
}

void
tprint_struct_next(void)
{
	if (structured_output) {
		json_close_struct_field();
		json_stack_set_needs_sep(JSON_CONTAINER_OBJECT);
	} else {
		STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
		STRACE_PRINTS(", ");
		STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
	}
}

void
tprint_struct_end(void)
{
	if (structured_output) {
		json_close_struct_field();
		tprint_object_end();
	} else {
		STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
		STRACE_PRINTS("}");
		STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
	}
}

void
tprint_union_begin(void)
{
	tprint_struct_begin();
}

void
tprint_union_next(void)
{
	tprint_struct_next();
}

void
tprint_union_end(void)
{
	tprint_struct_end();
}

void
tprint_array_begin(void)
{
	if (structured_output) {
		tprints_object_field_string("type", "array");
		tprints_object_field_begin("elems");
		json_print_array_begin();
	} else {
		STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
		STRACE_PRINTS("[");
		STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
	}
}

void
json_print_array_begin(void)
{
	if (!structured_output)
		return;

	STRACE_PRINTS(JSON_ARR_BEGIN);
	json_stack_push(JSON_CONTAINER_ARRAY);
}

void
json_print_array_element_begin(void)
{
	if (!structured_output)
		return;

	if (json_stack_needs_sep(JSON_CONTAINER_ARRAY))
		STRACE_PRINTS(JSON_SEP);
}

void
json_print_array_element_end(void)
{
	if (!structured_output)
		return;

	json_stack_set_needs_sep(JSON_CONTAINER_ARRAY);
}

void
tprint_array_next(void)
{
	if (structured_output) {
		json_stack_set_needs_sep(JSON_CONTAINER_ARRAY);
		json_print_array_element_begin();
	} else {
		STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
		STRACE_PRINTS(", ");
		STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
	}
}

void
tprint_array_end(void)
{
	if (structured_output) {
		json_print_array_end();
		tprint_object_field_end();
	} else {
		STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
		STRACE_PRINTS("]");
		STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
	}
}

void
json_print_array_end(void)
{
	if (!structured_output)
		return;

	if (structured_output->state.depth == 0
	    || json_stack_top_type() != JSON_CONTAINER_ARRAY)
		error_func_msg_and_die("structured output array"
				       " end outside of an array");
	STRACE_PRINTS(JSON_ARR_END);
	json_stack_pop();
}

void
tprint_array_fetch_failed_addr(unsigned long long addr)
{
	(void) addr;
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

/* Emit a NUL-terminated string as a "field": [b0, b1, ...] decimal array. */
static void
tprints_object_field_bytes(const char *field, const char *str)
{
	const unsigned char *ustr = (const unsigned char *) str;

	tprints_object_field_begin(field);
	json_print_array_begin();
	for (size_t i = 0; ustr[i]; ++i) {
		json_print_array_element_begin();
		STRACE_PRINTF("%u", ustr[i]);
		json_print_array_element_end();
	}
	json_print_array_end();
	tprint_object_field_end();
}

void
tprints_event_pid(int pid, const char *comm)
{
	tprints_object_field_begin("pid");
	tprint_object_begin();
	tprints_object_field_string("type", "tid");
	tprints_object_field_begin("raw");
	tprintf_string_value("%d", pid);
	tprint_object_field_end();
	if (comm && *comm) {
		tprints_object_field_begin("pid_info");
		tprint_object_begin();
		tprints_object_field_bytes("comm", comm);
		tprint_object_end();
		tprint_object_field_end();
	}
	tprint_object_end();
	tprint_object_field_end();
}

static void
emit_syscall_event_start(const char *name, kernel_ulong_t scno, int entry,
			 int pid, const char *comm)
{
	tprintf_event_start("syscall");
	tprints_event_pid(pid, comm);
	tprints_object_field_begin("syscall");
	tprint_object_begin();
	tprints_object_field_string("type", "syscall");
	tprints_object_field_begin("name");
	tprintf_string_value("%s", name);
	tprint_object_field_end();
	if (scno != (kernel_ulong_t) -1) {
		json_prints_object_field_begin("scno");
		tprintf_string_value("%llu",
				      (unsigned long long) scno);
		json_print_object_field_end();
	}
	tprint_object_end();
	tprint_object_field_end();
	tprints_object_field_begin("entering");
	STRACE_PRINTS(entry ? JSON_TRUE : JSON_FALSE);
	tprint_object_field_end();
	tprints_object_field_begin("args");
	json_print_array_begin();
	if (entry)
		structured_output->arg_index = 0;
	structured_output->arg_open = false;
}

void
tprints_arg_begin(const char *name, unsigned long long scno, bool entering,
		  int pid, const char *comm)
{
	if (structured_output) {
		emit_syscall_event_start(name, scno, entering, pid, comm);
	} else {
		STRACE_PRINT_COLOR_SEQ(COLOR_SYSCALL);
		tprints_syscall_name(name);
		STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
		STRACE_PRINTS("(");
		STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
	}
}

static void
close_arg(void)
{
	if (!structured_output)
		return;

	if (!structured_output->arg_open)
		return;
	structured_output->arg_open = false;

	while (structured_output->state.depth
	       > structured_output->arg_open_depth) {
		tprint_object_end();
	}

	tprint_object_end();
	json_print_array_element_end();
}

void
tprint_arg_next(void)
{
	if (structured_output) {
		close_arg();
		return;
	}

	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(", ");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_arg_end(void)
{
	if (structured_output) {
		close_arg();
		json_print_array_end();
		json_print_object_field_end();
	} else {
		STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
		STRACE_PRINTS(")");
		STRACE_PRINT_COLOR_SEQ(COLOR_RESET);
	}
}

static void
tprints_arg_name_unconditionally_at(unsigned int index, const char *name)
{
	if (structured_output) {
		close_arg();

		json_print_array_element_begin();
		tprint_object_begin();
		tprints_object_field_begin("index");
		STRACE_PRINTF("%u", index);
		tprint_object_field_end();
		tprints_object_field_begin("arg");
		tprintf_string_value("%s", name);
		tprint_object_field_end();
		structured_output->arg_open = true;
		structured_output->arg_open_depth =
			structured_output->state.depth;
	} else {
		STRACE_PRINT_COLOR_SEQ(COLOR_ARGNAME);
		STRACE_PRINTF("%s", name);
		STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
		STRACE_PRINTS("=");
		STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
	}
}

void
tprints_arg_name_unconditionally(const char *name)
{
	unsigned int index =
		structured_output ? structured_output->arg_index++ : 0;
	tprints_arg_name_unconditionally_at(index, name);
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
	if (Nflag || structured_output)
		tprints_arg_name_unconditionally(name);
}

void
tprints_arg_name_at(unsigned int index, const char *name)
{
	if (Nflag || structured_output)
		tprints_arg_name_unconditionally_at(index, name);
}

void
tprints_arg_next_name(const char *name)
{
	tprint_arg_next();
	tprints_arg_name(name);
}

void
tprints_arg_next_name_at(unsigned int index, const char *name)
{
	tprint_arg_next();
	tprints_arg_name_at(index, name);
}

void
json_print_arg_inout(void)
{
	if (!structured_output)
		return;

	tprints_object_field_begin("inout");
	STRACE_PRINTS(JSON_TRUE);
	tprint_object_field_end();
}

void
json_print_arg_changed(void)
{
	if (!structured_output)
		return;

	tprints_object_field_begin("changed");
	STRACE_PRINTS(JSON_TRUE);
	tprint_object_field_end();
}

void
tprints_fn_begin(const char *name)
{
	if (structured_output)
		return;

	STRACE_PRINT_COLOR_SEQ(COLOR_CALL);
	STRACE_PRINTF("%s", name);
	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("(");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_fn_next(void)
{
	if (structured_output)
		return;

	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(", ");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_fn_end(void)
{
	if (structured_output)
		return;

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
	if (structured_output)
		return;

	STRACE_PRINT_COLOR_SEQ(COLOR_COMMENT);
	STRACE_PRINTS(" /* ");
}

void
tprint_comment_end(void)
{
	if (structured_output)
		return;

	STRACE_PRINT_COLOR_SEQ(COLOR_COMMENT);
	STRACE_PRINTS(" */");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_indirect_begin(void)
{
	if (structured_output)
		return;

	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("[");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_indirect_end(void)
{
	if (structured_output) {
		tprints_object_field_begin("indirect");
		STRACE_PRINTS(JSON_TRUE);
		tprint_object_field_end();
	} else {
		STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
		STRACE_PRINTS("]");
		STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
	}
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
	if (structured_output)
		return;

	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("<");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_associated_info_end(void)
{
	if (structured_output)
		return;

	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(">");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_more_data_follows(void)
{
	if (structured_output)
		return;

	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("...");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_value_changed(void)
{
	if (structured_output)
		return;

	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS(" => ");
	STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
}

void
tprint_alternative_value(void)
{
	if (structured_output) {
		tprint_array_next();
	} else {
		STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
		STRACE_PRINTS(" or ");
		STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
	}
}

void
tprint_unavailable(void)
{
	if (structured_output) {
		json_prints_object_field_string("type", "unavailable");
	} else {
		STRACE_PRINT_COLOR_SEQ(COLOR_ERROR);
		STRACE_PRINTS("???");
		STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
	}
}

void
tprint_flags_or(void)
{
	if (structured_output)
		return;

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
	if (structured_output) {
		tprints_object_field_begin(name);
	} else {
		STRACE_PRINT_COLOR_SEQ(COLOR_ARGNAME);
		STRACE_PRINTF("%s", name);
		STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
		STRACE_PRINTS("=");
		STRACE_PRINT_COLOR_SEQ(COLOR_ARGVAL);
	}
	tprint_object_begin();
}

void
tprint_sysret_begin(void)
{
	if (structured_output) {
		tprints_object_field_begin("return");
		tprint_object_begin();
		return;
	}

	STRACE_PRINT_COLOR_SEQ(COLOR_PUNCT);
	STRACE_PRINTS("=");
	STRACE_PRINT_COLOR_SEQ(COLOR_RESET);
}

void
tprints_sysret_next(const char *name)
{
	if (structured_output)
		return;

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
tprints_sysret_string(const char *name, const char *str, bool parentheses)
{
	tprints_sysret_next(name);
	if (structured_output)
		tprints_object_field_string(name, str);
	else if (parentheses)
		STRACE_PRINTF("(%s)", str);
	else
		STRACE_PRINTS(str);
}

void
tprint_sysret_end(void)
{
	if (structured_output) {
		tprint_object_end();
		tprint_object_field_end();
		return;
	}

	STRACE_PRINT_COLOR_SEQ(COLOR_RESET);
}

void
tprint_sysret_errno(unsigned long u_error, const char *sym, const char *str)
{
	if (structured_output) {
		json_prints_object_field_begin("errno");
		tprint_object_begin();
		tprints_object_type("errno");
		tprintf_object_field_string("raw", "%lu", u_error);
		if (sym)
			json_prints_object_field_string("sym", sym);
		json_prints_object_field_string("strerror", str);
		tprint_object_end();
		json_print_object_field_end();
	} else if (sym) {
		tprints_sysret_next("error");
		STRACE_PRINTS(sym);
		tprints_sysret_string("strerror", str, true);
	} else {
		tprints_sysret_next("strerror");
		STRACE_PRINTF("(errno %lu)", u_error);
	}
}

void
trad_prints(const char *s)
{
	if (!structured_output)
		STRACE_PRINTS(s);
}

void
tprintf_event_start(const char *type)
{
	if (!structured_output)
		return;

	json_stack_reset();
	tprint_object_begin();
	tprints_object_field_begin("event");
	tprintf_string_value("%s", type);
	tprint_object_field_end();
}

void
tprint_event_end(void)
{
	if (!structured_output)
		return;

	STRACE_PRINTS(JSON_OBJ_END);
	json_stack_pop();
}
