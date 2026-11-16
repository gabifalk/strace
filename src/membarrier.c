/*
 * Copyright (c) 2015 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2015-2025 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"

#include "xlat/membarrier_cmds.h"
#include "xlat/membarrier_flags.h"

static void
print_membarrier_result(struct tcb *const tcp, const unsigned long long val)
{
	tprint_sysret_value_begin(val, true);
	tprints_sysret_result_note("");
	printflags(membarrier_cmds, val, NULL);
	tprint_sysret_result_end();
}

SYS_FUNC(membarrier)
{
	if (entering(tcp)) {
		int cmd = tcp->u_arg[0];
		unsigned flags = tcp->u_arg[1];
		int cpu_id = tcp->u_arg[2];

		/* cmd */
		tprints_arg_name("cmd");
		printxval(membarrier_cmds, cmd, "MEMBARRIER_CMD_???");

		/* flags */
		tprints_arg_next_name("flags");
		printflags(membarrier_flags, flags, "MEMBARRIER_CMD_FLAG_???");

		if (flags & MEMBARRIER_CMD_FLAG_CPU) {
			/* cpu_id */
			tprints_arg_next_name("cpu_id");
			PRINT_VAL_D(cpu_id);
		}

		return cmd ? RVAL_DECODED : 0;
	}

	if (syserror(tcp) || !tcp->u_rval)
		return 0;

	tcp->rval_result_printer = print_membarrier_result;
	return RVAL_RESULT;
}
