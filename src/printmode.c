/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993-1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 2012 Denys Vlasenko <vda.linux@googlemail.com>
 * Copyright (c) 2012-2023 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"

#include <fcntl.h>
#include <sys/stat.h>

#include "xlat/modetypes.h"

void
print_symbolic_mode_t(const unsigned int mode)
{
	const char *ifmt = "";

	if (mode & S_IFMT)
		ifmt = xlookup(modetypes, mode & S_IFMT);

	tprint_symbolic_mode(mode, ifmt);
}

void
print_numeric_umode_t(const unsigned short mode)
{
	tprint_numeric_mode(mode);
}

void
print_numeric_ll_umode_t(const unsigned long long mode)
{
	tprint_numeric_mode(mode);
}
