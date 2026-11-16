/*
 * Check JSONL output of the setgroups syscall.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_setgroups

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>

# if defined __NR_setgroups32 && __NR_setgroups != __NR_setgroups32
#  define GID_TYPE	unsigned short
# else
#  define GID_TYPE	unsigned int
# endif

# define MAX_STRLEN 2

static bool merged;

/*
 * The list is printed as an array of size elements when print_array is set,
 * and as a pointer otherwise; only the first readable elements of the list
 * can be read.
 */
static void
test_setgroups(kernel_ulong_t size, const GID_TYPE *list, bool print_array,
	       unsigned int readable)
{
	const char *side = merged ? "entry" : NULL;
	long rc = syscall(__NR_setgroups, size, list);
	int saved_errno = errno;

	if (merged)
		jsonl_syscall_merged_open("setgroups", __NR_setgroups);
	else
		jsonl_syscall_open("setgroups", __NR_setgroups, true);

	jsonl_arg_open(0, "size", side);
	jsonl_decimal((int) size);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(1, "list", side);
	if (print_array) {
		unsigned int i;

		jsonl_array_open();
		for (i = 0; i < (unsigned int) size; ++i) {
			if (i >= readable || i >= MAX_STRLEN)
				break;
			if (i)
				printf(", ");
			printf("{");
			jsonl_gid(list[i] == (GID_TYPE) -1U ? -1U : list[i]);
			printf("}");
		}
		if (i < (unsigned int) size && i >= readable) {
			jsonl_array_fetch_failed(list + i);
		} else {
			jsonl_array_close();
			if (i < (unsigned int) size)
				jsonl_truncated();
		}
	} else {
		jsonl_addr(list);
	}
	jsonl_arg_close(side);

	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("setgroups", __NR_setgroups, false);
	}
	printf("], ");
	errno = saved_errno;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	/* check how the first argument is decoded */
	test_setgroups(0, NULL, false, 0);
	test_setgroups(F8ILL_KULONG_MASK, NULL, false, 0);
	test_setgroups(1, NULL, false, 0);
	test_setgroups((kernel_ulong_t) 0xffffffff00000001ULL, NULL, false, 0);
	test_setgroups(-1U, NULL, false, 0);
	test_setgroups(-1L, NULL, false, 0);

	/* check how the second argument is decoded */
	TAIL_ALLOC_OBJECT_CONST_PTR(GID_TYPE, g1);
	TAIL_ALLOC_OBJECT_CONST_ARR(GID_TYPE, g2, 2);
	TAIL_ALLOC_OBJECT_CONST_ARR(GID_TYPE, g3, 3);

	*g1 = (GID_TYPE) -1U;
	test_setgroups(0, g1 + 1, true, 0);
	test_setgroups(1, g1, true, 1);
	test_setgroups(1, g1 + 1, false, 0);
	test_setgroups(1, (const GID_TYPE *) -1L, false, 0);
	test_setgroups(2, g1, true, 1);

	g2[0] = -2;
	g2[1] = -3;
	test_setgroups(2, g2, true, 2);
	test_setgroups(3, g2, true, 2);

	g3[0] = 0;
	g3[1] = 1;
	g3[2] = 2;
	test_setgroups(3, g3, true, 3);
	test_setgroups(4, g3, true, 3);

	long rc = sysconf(_SC_NGROUPS_MAX);
	const unsigned ngroups_max = rc;

	if ((unsigned long) rc == ngroups_max && (int) ngroups_max > 0) {
		test_setgroups(ngroups_max, g3, true, 3);
		test_setgroups(F8ILL_KULONG_MASK | ngroups_max, g3, true, 3);
		test_setgroups(ngroups_max + 1, g3, false, 0);
	}

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_setgroups")

#endif
