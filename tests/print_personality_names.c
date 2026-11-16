/*
 * Print the names of the personalities supported by strace as a JSON array.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "arch_defs.h"
#include <stdio.h>

int
main(void)
{
	static const char *const names[] = PERSONALITY_NAMES;

	printf("[");
	for (size_t i = 0; i < ARRAY_SIZE(names); ++i)
		printf("%s\"%s\"", i ? ", " : "", names[i]);
	printf("]\n");
	return 0;
}
