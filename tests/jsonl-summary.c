/*
 * Check JSONL output of the summary and summary_total events.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_chdir && defined __NR_getpid

# include "jsonl.h"

# include <stdio.h>
# include <unistd.h>

static void
print_row(unsigned int personality, const char *syscall, unsigned int calls,
	  unsigned int errors)
{
	if (syscall)
		printf("{\"event\": \"summary\", ");
	else
		printf("{\"event\": \"summary_total\", ");
	printf("\"personality\": %u, ", personality);
	printf("\"columns\": [\"syscall\", \"calls\", \"errors\"], ");
	if (syscall)
		printf("\"syscall\": \"%s\", ", syscall);
	printf("\"calls\": \"%u\", \"errors\": \"%u\", "
	       "\"time_percent\": \"%s\", \"total_time\": \"N\", "
	       "\"avg_time\": \"N\", "
	       "\"min_time\": \"N\", \"max_time\": \"N\"}\n",
	       calls, errors, syscall ? "N" : "100.00");
}

int
main(void)
{
	syscall(__NR_chdir, ".");
	syscall(__NR_chdir, "");
	syscall(__NR_chdir, "");
	syscall(__NR_getpid);
	syscall(__NR_getpid);

# ifdef __x86_64__
	__asm__ volatile("movl $200, %%eax; int $0x80"
			 ::: "eax", "r8", "r9", "r10", "r11",
			 "r12", "r13", "r14", "r15", "memory");
#  ifdef MPERS_IS_mx32
	print_row(1, "getgid32", 1, 0);
	print_row(1, NULL, 1, 0);
#  endif
# endif

	print_row(JSONL_PERSONALITY, "chdir", 3, 2);
	print_row(JSONL_PERSONALITY, "getpid", 2, 0);
	print_row(JSONL_PERSONALITY, NULL, 5, 2);

# if defined __x86_64__ && !defined MPERS_IS_mx32
	print_row(1, "getgid32", 1, 0);
	print_row(1, NULL, 1, 0);
# endif

	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_chdir && __NR_getpid")

#endif
