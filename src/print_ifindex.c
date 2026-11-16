/*
 * Copyright (c) 2001-2021 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"
#include <net/if.h>

#ifdef HAVE_IF_INDEXTONAME

# define IFNAME_QUOTED_SZ (IFNAMSIZ * 4 + 3)

const char *
get_ifname(const unsigned int ifindex)
{
	static char name_quoted_buf[IFNAME_QUOTED_SZ];
	char name_buf[IFNAMSIZ];

	if (!if_indextoname(ifindex, name_buf))
		return NULL;

	if (string_quote(name_buf, name_quoted_buf, sizeof(name_buf),
			 QUOTE_0_TERMINATED | QUOTE_OMIT_LEADING_TRAILING_QUOTES,
			 NULL))
		return NULL;

	return name_quoted_buf;
}

#else /* !HAVE_IF_INDEXTONAME */

const char *get_ifname(const unsigned int ifindex) { return NULL; }

#endif /* HAVE_IF_INDEXTONAME */

void
print_ifindex(const unsigned int ifindex)
{
	tprint_ifindex(ifindex, get_ifname(ifindex));
}
