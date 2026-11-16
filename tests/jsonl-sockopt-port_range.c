/*
 * Check JSONL output of the IP_LOCAL_PORT_RANGE socket option:
 * a port_range value with lo and hi ports when the range is
 * well-formed.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"
#include <netinet/in.h>

#if defined __NR_setsockopt && defined IP_LOCAL_PORT_RANGE

# include "jsonl_setsockopt.h"

# include <string.h>
# include <sys/socket.h>

static void
print_port_range(const void *val)
{
	const unsigned int ports = *(const unsigned int *) val;
	const unsigned short lo = ports & 0xffff;
	const unsigned short hi = ports >> 16;

	printf("\"type\": \"port_range\", \"raw\": \"%#x\"", ports);
	if (ports && (!lo || !hi || lo <= hi))
		printf(", \"lo\": \"%hu\", \"hi\": \"%hu\"", lo, hi);
	jsonl_indirect();
}

int
main(int argc, char *argv[])
{
	jsonl_setsockopt_merged = argc > 1 && !strcmp(argv[1], "merged");

	(void) close(0);
	if (socket(AF_INET, SOCK_STREAM, 0))
		perror_msg_and_skip("socket");

	TAIL_ALLOC_OBJECT_CONST_PTR(unsigned int, ports);
	static const unsigned int values[] = {
		0x10000400, 0x400, 0x10000000, 0x04001000, 0
	};

	for (size_t i = 0; i < ARRAY_SIZE(values); ++i) {
		*ports = values[i];
		jsonl_setsockopt(0, ARG_STR(SOL_IP),
				 ARG_STR(IP_LOCAL_PORT_RANGE),
				 ports, sizeof(*ports), print_port_range);
	}

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_setsockopt && IP_LOCAL_PORT_RANGE")

#endif
