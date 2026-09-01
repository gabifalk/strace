/*
 * Copyright (c) 2016-2017 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2017-2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdarg.h>
#include "print_fields.h"

void
tprints_field_name(const char *name)
{
	STRACE_PRINTF("%s=", name);
}

ATTRIBUTE_FORMAT((printf, 2, 0))
void
tprintv_object_field_int(const char *name, const char *fmt, va_list args)
{
	(void) name;
	STRACE_PRINTV(fmt, args);
}

ATTRIBUTE_FORMAT((printf, 2, 3))
void
tprintf_object_field_int(const char *name, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	tprintv_object_field_int(name, fmt, args);
	va_end(args);
}

void
json_print_object_begin(void)
{
}

void
json_print_object_end(void)
{
}

void
json_prints_object_field_begin(const char *field)
{
	(void) field;
}

void
json_prints_object_field_string(const char *field, const char *s)
{
	(void) field;
	(void) s;
}

void
json_print_object_field_end(void)
{
}

void
tprint_array_begin(void)
{
	STRACE_PRINTS("[");
}

void
tprint_array_next(void)
{
	STRACE_PRINTS(", ");
}

void
tprint_array_end(void)
{
	STRACE_PRINTS("]");
}
