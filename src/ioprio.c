/*
 * Copyright (c) 2015 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2014-2025 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"
#include "xstring.h"

#include "xlat/ioprio_who.h"
#include "xlat/ioprio_class.h"

#define IOPRIO_CLASS_SHIFT	(13)
#define IOPRIO_PRIO_MASK	((1ul << IOPRIO_CLASS_SHIFT) - 1)

#define IOPRIO_PRIO_CLASS(mask)	((mask) >> IOPRIO_CLASS_SHIFT)
#define IOPRIO_PRIO_DATA(mask)	((mask) & IOPRIO_PRIO_MASK)

static const char *
sprint_ioprio(unsigned int ioprio)
{
	static char outstr[256];
	char class_buf[64];
	unsigned int class, data;

	class = IOPRIO_PRIO_CLASS(ioprio);
	data = IOPRIO_PRIO_DATA(ioprio);
	sprintxval(class_buf, sizeof(class_buf), ioprio_class, class,
		   "IOPRIO_CLASS_???");
	xsprintf(outstr, "IOPRIO_PRIO_VALUE(%s, %d)", class_buf, data);

	return outstr;
}

static void
print_ioprio_value(unsigned int ioprio, bool signed_raw,
		   enum xlat_style style)
{
	tprint_ioprio(ioprio, signed_raw, sprint_ioprio(ioprio), ioprio_class,
		      IOPRIO_PRIO_CLASS(ioprio), IOPRIO_PRIO_DATA(ioprio),
		      style);
}

void
print_ioprio(unsigned int ioprio)
{
	print_ioprio_value(ioprio, false, XLAT_STYLE_DEFAULT);
}

static void
print_ioprio_rval(const unsigned long long val, const enum xlat_style style)
{
	print_ioprio_value(val, false, style);
}

static void
ioprio_print_who(struct tcb *tcp, int which, int who)
{
	switch (which)
	{
	case IOPRIO_WHO_PROCESS:
		printpid(tcp, who, PT_TGID);
		break;
	case IOPRIO_WHO_PGRP:
		printpid(tcp, who, PT_PGID);
		break;
	default:
		PRINT_VAL_D(who);
		break;
	}
}

SYS_FUNC(ioprio_get)
{
	if (entering(tcp)) {
		/* which */
		tprints_arg_name("which");
		printxval(ioprio_who, tcp->u_arg[0], "IOPRIO_WHO_???");

		/* who */
		tprints_arg_next_name("who");
		ioprio_print_who(tcp, tcp->u_arg[0], tcp->u_arg[1]);
		return 0;
	} else {
		if (syserror(tcp))
			return 0;
		tcp->rval_printer = print_ioprio_rval;
		return RVAL_PRINTER;
	}
}

SYS_FUNC(ioprio_set)
{
	/* which */
	tprints_arg_name("which");
	printxval(ioprio_who, tcp->u_arg[0], "IOPRIO_WHO_???");

	/* who */
	tprints_arg_next_name("who");
	ioprio_print_who(tcp, tcp->u_arg[0], tcp->u_arg[1]);

	/* ioprio */
	tprints_arg_next_name("ioprio");
	print_ioprio_value(tcp->u_arg[2], true, XLAT_STYLE_DEFAULT);

	return RVAL_DECODED;
}
