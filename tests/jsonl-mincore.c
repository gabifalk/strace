/*
 * Check JSONL output of the mincore syscall.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_mincore

# include "jsonl.h"

# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

static bool merged;

static long
k_mincore(const void *addr, size_t size, const void *vec)
{
	return syscall(__NR_mincore, addr, size, vec);
}

/* pages == 0 means the vector is printed as a pointer. */
static void
emit(const void *addr, size_t size, const unsigned char *vec,
     unsigned int pages, long rc)
{
	const char *side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("mincore", __NR_mincore);
	else
		jsonl_syscall_open("mincore", __NR_mincore, true);

	jsonl_arg_open(0, "addr", side);
	jsonl_addr(addr);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(1, "length", side);
	jsonl_unsigned(size);
	jsonl_arg_close(side);

	if (merged) {
		printf(", ");
	} else {
		printf("]}\n");
		jsonl_syscall_open("mincore", __NR_mincore, false);
	}

	side = merged ? "exit" : NULL;
	jsonl_arg_open(2, "vec", side);
	if (pages) {
		jsonl_array_open();
		for (unsigned int i = 0; i < pages && i < DEFAULT_STRLEN; ++i) {
			printf("%s{", i ? ", " : "");
			jsonl_unsigned(vec[i] & 1);
			printf("}");
		}
		jsonl_array_close();
		if (pages > DEFAULT_STRLEN)
			jsonl_truncated();
	} else {
		jsonl_addr(vec);
	}
	jsonl_arg_close(side);

	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
}

static void
check_mincore(const unsigned int pages, const void *const addr,
	      const size_t size, unsigned char *const vec)
{
	long rc = k_mincore(addr, size, vec);
	if (rc)
		perror_msg_and_skip("mincore");

	emit(addr, size, vec, pages, rc);
}

static void
test_mincore(const unsigned int pages)
{
	const size_t page_size = get_page_size();
	const size_t size = pages * page_size;
	void *const addr = tail_alloc(size);
	unsigned char *const vec = tail_alloc(pages);
	long rc;

	rc = k_mincore(addr, size, NULL);
	emit(addr, size, NULL, 0, rc);

	rc = k_mincore(addr, size, vec + pages);
	emit(addr, size, vec + pages, 0, rc);

	check_mincore(pages, addr, size, vec);
	check_mincore(pages, addr, size - page_size + 1, vec);
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	test_mincore(1);
	test_mincore(2);
	test_mincore(DEFAULT_STRLEN);
	test_mincore(DEFAULT_STRLEN + 1);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_mincore")

#endif
