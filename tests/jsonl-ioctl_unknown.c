/*
 * Check JSONL output of the argument of ioctl commands of unknown types:
 * an argument printed again on exiting, and an argument printed
 * on exiting only.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_ioctl

# include "jsonl_ioctl.h"

# include <stdio.h>
# include <string.h>

static void
bytes(const char *buf, size_t size)
{
	printf("\"type\": \"bytes\", \"value\": ");
	jsonl_bytes_n(buf, size);
}

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	static const char data[] = { 1, 2, 3, 4 };
	char *const buf = tail_memdup(data, sizeof(data));

	jsonl_ioctl_call(_IOWR(0xde, 0xad, 4), NULL, NULL, buf);
	jsonl_ioctl_argp_open("argp", false);
	bytes(buf, sizeof(data));
	printf(", \"inout\": true");
	if (jsonl_ioctl_merged) {
		printf("}, \"exit\": {");
		jsonl_ioctl_exiting = true;
	} else {
		jsonl_ioctl_argp_close();
		jsonl_ioctl_argp_open("argp", true);
	}
	printf("\"changed\": true, ");
	bytes(buf, sizeof(data));
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_ioctl_call(_IOR(0xde, 0xad, 4), NULL, NULL, buf);
	jsonl_ioctl_argp_open("argp", true);
	bytes(buf, sizeof(data));
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
