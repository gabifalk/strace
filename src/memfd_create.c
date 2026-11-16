/*
 * Copyright (c) 2015 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2015-2025 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"
#include <linux/memfd.h>
#include "xlat/memfd_create_flags.h"

#ifndef MFD_NAME_MAX_LEN
# define MFD_NAME_MAX_LEN (255 - (sizeof("memfd:") - 1))
#endif

SYS_FUNC(memfd_create)
{
	/* name */
	tprints_arg_name("name");
	printstr_ex(tcp, tcp->u_arg[0], MFD_NAME_MAX_LEN + 1,
		    QUOTE_0_TERMINATED);

	tprints_arg_next_name("flags");
	unsigned int flags = tcp->u_arg[1];

	if (!flags) {
		PRINT_VAL_X(flags);
		return RVAL_DECODED | RVAL_FD;
	}

	if (!tprint_xlat_flags_value_begin(flags))
		return RVAL_DECODED | RVAL_FD;

	const unsigned int mask = MFD_HUGE_MASK << MFD_HUGE_SHIFT;
	const unsigned int hugetlb_value = flags & mask;
	flags &= ~mask;

	if (flags || !hugetlb_value)
		printflags_ex(flags, "MFD_???", XLAT_STYLE_ABBREV,
			      memfd_create_flags, NULL);

	if (hugetlb_value) {
		if (flags)
			tprint_flags_value_or();
		tprints_shift_value(hugetlb_value,
				    hugetlb_value >> MFD_HUGE_SHIFT,
				    "MFD_HUGE_SHIFT");
	}

	tprint_xlat_flags_value_end();

	return RVAL_DECODED | RVAL_FD;
}
