/*
 * Device number printing routine.
 *
 * Copyright (c) 2016-2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"
#include <sys/sysmacros.h>

void
print_dev_t(const unsigned long long dev)
{
	tprint_dev_t(dev, major(dev), minor(dev));
}
