/*
 * Check JSONL output of -e status=none syscall filtering.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"

int
main(void)
{
	jsonl_exit_event(0);
	return 0;
}
