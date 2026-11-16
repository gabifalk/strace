/*
 * Kernel version printing routine.
 *
 * Copyright (c) 2018-2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"

void
print_kernel_version(const unsigned long version)
{
	tprint_kernel_version(version);
}
