/*
 * Check JSONL output of the ICMP_FILTER socket option: a bitset value
 * listing the filtered ICMP types, or the types that are not filtered
 * when more than half of them are.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_setsockopt

# include "jsonl_setsockopt.h"

# include <string.h>
# include <sys/socket.h>
# include <linux/icmp.h>

static const char *const icmp_types[] = {
	[ICMP_ECHOREPLY] = "ICMP_ECHOREPLY",
	[ICMP_DEST_UNREACH] = "ICMP_DEST_UNREACH",
	[ICMP_ECHO] = "ICMP_ECHO",
	[ICMP_TIMESTAMP] = "ICMP_TIMESTAMP",
};

static void
print_icmp_filter(const void *val)
{
	const uint32_t data = ((const struct icmp_filter *) val)->data;
	const bool inverted = __builtin_popcount(data) > 16;
	const uint32_t members = inverted ? ~data : data;
	bool first = true;

	printf("\"type\": \"bitset\", \"raw\": \"%#x\"", data);
	if (inverted)
		printf(", \"inverted\": true");
	printf(", \"elems\": [");
	for (unsigned int i = 0; i < 32; ++i) {
		if (!(members & (1U << i)))
			continue;
		printf("%s{", first ? "" : ", ");
		jsonl_const(i, true, icmp_types[i], NULL);
		printf("}");
		first = false;
	}
	printf("]");
}

int
main(int argc, char *argv[])
{
	jsonl_setsockopt_merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_PTR(struct icmp_filter, filter);
	const uint32_t values[] = {
		0,
		1U << ICMP_ECHOREPLY | 1U << ICMP_DEST_UNREACH,
		~(1U << ICMP_ECHO | 1U << ICMP_TIMESTAMP),
		~0U,
	};

	for (size_t i = 0; i < ARRAY_SIZE(values); ++i) {
		filter->data = values[i];
		jsonl_setsockopt(-1, ARG_STR(SOL_RAW), ARG_STR(ICMP_FILTER),
				 filter, sizeof(*filter), print_icmp_filter);
	}

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_setsockopt")

#endif
