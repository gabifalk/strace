/*
 * Check JSONL output of the getrandom syscall.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"
#include "scno.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/random.h>

#define MAX_STRLEN 3

static bool merged;
static bool skip_entering;

static void
arg_open(unsigned int index, const char *name)
{
	if (index)
		printf(", ");
	jsonl_arg_open(index, name, merged ? "exit" : NULL);
}

static void
arg_close(void)
{
	jsonl_arg_close(merged ? "exit" : NULL);
}

/* The buffer is printed as a pointer when the syscall has failed. */
static void
print_getrandom(const void *buf, size_t len, unsigned int flags, long rc)
{
	static const struct strval32 known[] = {
		{ ARG_STR(GRND_NONBLOCK) },
		{ ARG_STR(GRND_RANDOM) }
	};
	int saved_errno = errno;

	if (merged) {
		jsonl_syscall_merged_open("getrandom", __NR_getrandom);
	} else {
		if (!skip_entering) {
			jsonl_syscall_open("getrandom", __NR_getrandom, true);
			printf("]}\n");
		}
		skip_entering = false;
		jsonl_syscall_open("getrandom", __NR_getrandom, false);
	}

	arg_open(0, "buf");
	if (rc < 0 || !buf)
		jsonl_addr(buf);
	else
		jsonl_bytes_value(buf, MIN((size_t) rc, MAX_STRLEN),
				  (size_t) rc > MAX_STRLEN);
	arg_close();

	arg_open(1, "buflen");
	jsonl_unsigned(len);
	arg_close();

	arg_open(2, "flags");
	jsonl_flags(flags, known, ARRAY_SIZE(known), NULL);
	arg_close();

	printf("], ");
	errno = saved_errno;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

static void
test_getrandom(const void *buf, size_t len, unsigned int flags, long expected)
{
	long rc = syscall(__NR_getrandom, buf, len, flags);

	if (rc != expected)
		perror_msg_and_skip("getrandom");
	print_getrandom(buf, len, flags, rc);
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	/*
	 * libc calls getrandom before main, and in split mode the entering
	 * event of that call is the same as the one of the first call below,
	 * so the expected output starts with the exiting event of the latter.
	 */
	skip_entering = !merged;

	unsigned char buf[4];

	/*
	 * The first call is printed before it is made, like in getrandom.c,
	 * so that glibc calls getrandom to initialize its tcache before it.
	 */
	print_getrandom(NULL, 0, 0, 0);
	if (syscall(__NR_getrandom, NULL, 0, 0) != 0)
		perror_msg_and_skip("getrandom");
	test_getrandom(buf, sizeof(buf) - 1, 0, sizeof(buf) - 1);
	test_getrandom(buf, sizeof(buf), GRND_NONBLOCK, sizeof(buf));
	test_getrandom(buf, sizeof(buf), 0x3003, -1);

	jsonl_exit_event(0);
	return 0;
}
