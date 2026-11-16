/*
 * SPARC-specific syscall decoders.
 *
 * Copyright (c) 2018-2021 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"

#if defined SPARC || defined SPARC64

# include "xlat/sparc_kern_features.h"

static void
print_kern_features_result(struct tcb *const tcp,
			   const unsigned long long val)
{
	tprint_sysret_value_begin(val, true);
	tprints_sysret_result_note("");
	printflags(sparc_kern_features, val, NULL);
	tprint_sysret_result_end();
}

SYS_FUNC(kern_features)
{
	if (entering(tcp) || syserror(tcp))
		return 0;

	if (!tcp->u_rval)
		return RVAL_HEX;

	tcp->rval_result_printer = print_kern_features_result;
	return RVAL_RESULT;
}

#endif /* SPARC || SPARC64 */
