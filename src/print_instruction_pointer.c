/*
 * Copyright (c) 1999-2022 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"

void
print_instruction_pointer(struct tcb *tcp)
{
	kernel_ulong_t ip = 0;
	const bool known = get_instruction_pointer(tcp, &ip);

	tprint_instruction_pointer(known, ip, current_wordsize);
}
