/*
 * Check JSONL output of the personality of syscalls of a tracee switching
 * between 64-bit and 32-bit syscalls.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __x86_64__ && defined __NR_getgid \
 && !defined MPERS_IS_m32 && !defined MPERS_IS_mx32

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>

static bool merged;

static void
print_syscall_open(const char *name, long scno, unsigned int personality,
		   const char *entering)
{
	printf("{\"event\": \"syscall\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, "
	       "\"syscall\": {\"type\": \"syscall\", \"name\": \"%s\", "
	       "\"scno\": \"%ld\", \"personality\": %u}, ",
	       (int) getpid(), name, scno, personality);
	if (entering)
		printf("\"entering\": %s, ", entering);
	printf("\"args\": [");
}

static void
print_getgid(const char *name, long scno, unsigned int personality,
	     unsigned long rc)
{
	if (merged) {
		print_syscall_open(name, scno, personality, NULL);
	} else {
		print_syscall_open(name, scno, personality, "true");
		printf("]}\n");
		print_syscall_open(name, scno, personality, "false");
	}
	printf("], \"return\": {\"type\": \"gid\", \"raw\": \"%lu\"}}\n", rc);
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	unsigned long rc = syscall(__NR_getgid);
	unsigned long rc32;
	__asm__ volatile("movl $200, %%eax; int $0x80"
			 : "=a" (rc32)
			 :: "r8", "r9", "r10", "r11",
			 "r12", "r13", "r14", "r15", "memory");
	unsigned long rc2 = syscall(__NR_getgid);

	print_getgid("getgid", __NR_getgid, 0, rc);
	print_getgid("getgid32", 200, 1, (unsigned int) rc32);
	print_getgid("getgid", __NR_getgid, 0, rc2);
	jsonl_exit_event(0);

	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__x86_64__ && __NR_getgid && !MPERS_IS_m32"
		    " && !MPERS_IS_mx32")

#endif
