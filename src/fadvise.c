/*
 * Copyright (c) 2004 Ulrich Drepper <drepper@redhat.com>
 * Copyright (c) 2004 Roland McGrath <roland@redhat.com>
 * Copyright (c) 2007 Daniel Jacobowitz  <dan@codesourcery.com>
 * Copyright (c) 2009 Andreas Schwab <schwab@redhat.com>
 * Copyright (c) 2009 Kirill A. Shutemov <kirill@shutemov.name>
 * Copyright (c) 2011-2015 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2014-2025 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"

#include <fcntl.h>

#include "xlat/advise.h"

SYS_FUNC(fadvise64)
{
	/* fd */
	tprints_arg_name("fd");
	printfd(tcp, tcp->u_arg[0]);

	/* offset */
	tprints_arg_next_name("offset");
	unsigned int argn = print_arg_lld(tcp, 1);

	/* len */
	tprints_arg_next_name("len");
	kernel_ulong_t len = tcp->u_arg[argn++];
	PRINT_VAL_U(len);

	/* advice */
	tprints_arg_next_name("advice");
	printxval(advise, tcp->u_arg[argn], "POSIX_FADV_???");

	return RVAL_DECODED;
}

/* On these architectures advice is the second parameter, printed last. */
#if defined __ARM_EABI__ || defined AARCH64 || defined POWERPC || defined XTENSA
# define ARG_ADVICE	1
# define ARG_OFFSET	2
# define ARG_LEN	3
#else
# define ARG_OFFSET	1
# define ARG_LEN	2
# define ARG_ADVICE	3
#endif

SYS_FUNC(fadvise64_64)
{
	/* fd */
	tprints_arg_name("fd");
	printfd(tcp, tcp->u_arg[0]);

	/* offset */
	tprints_arg_next_name_at(ARG_OFFSET, "offset");
	unsigned int argn = print_arg_lld(tcp, 1);

	/* len */
	tprints_arg_next_name_at(ARG_LEN, "len");
	argn = print_arg_lld(tcp, argn);

	/* advice */
	tprints_arg_next_name_at(ARG_ADVICE, "advice");
#if defined __ARM_EABI__ || defined AARCH64 || defined POWERPC || defined XTENSA
	printxval(advise, tcp->u_arg[1], "POSIX_FADV_???");
#else
	printxval(advise, tcp->u_arg[argn], "POSIX_FADV_???");
#endif

	return RVAL_DECODED;
}
