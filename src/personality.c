/*
 * Copyright (c) 2015 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2014-2025 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"
#include "xlat/personality_flags.h"
#include "xlat/personality_types.h"


static void
print_personality(unsigned int pers)
{
	tprint_flags_begin();
	printxval(personality_types, pers & PER_MASK, "PER_???");
	pers &= ~PER_MASK;
	if (pers) {
		tprint_flags_or();
		printflags_in(personality_flags, pers, NULL);
	}
	tprint_flags_end();
}

static void
print_personality_result(struct tcb *const tcp, const unsigned long long val)
{
	tprint_sysret_value_begin(val, true);
	tprints_sysret_result_note("");
	print_personality(val);
	tprint_sysret_result_end();
}

SYS_FUNC(personality)
{
	if (entering(tcp)) {
		const unsigned int pers = tcp->u_arg[0];

		tprints_arg_name("persona");
		if (0xffffffff == pers)
			PRINT_VAL_X(0xffffffff);
		else
			print_personality(pers);
		return 0;
	}

	if (syserror(tcp))
		return 0;

	tcp->rval_result_printer = print_personality_result;
	return RVAL_RESULT;
}
