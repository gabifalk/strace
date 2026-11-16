/*
 * Check JSONL output of the shmget syscall: the flags groups of shmflg.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_shmget

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <sys/shm.h>

# ifndef SHM_HUGE_SHIFT
#  define SHM_HUGE_SHIFT 26
# endif
# ifndef SHM_NORESERVE
#  define SHM_NORESERVE 010000
# endif

static const unsigned int key = 0xfdb97531;
static bool merged;

static void
mode_group(unsigned int mode, bool first)
{
	printf("%s{\"raw\": ", first ? "" : ", ");
	jsonl_xlat_raw(mode);
	printf(", \"group_type\": \"mode\", \"elems\": [{\"type\": \"mode\","
	       " \"raw\": \"%#03o\"}]}", mode);
}

static void
test_shmget(unsigned int flags, unsigned int huge, unsigned int unknown,
	    unsigned int mode)
{
	const unsigned int shmflg = flags | huge << SHM_HUGE_SHIFT | unknown
				    | mode;
	long rc = syscall(__NR_shmget, key, 1, shmflg);
	const int saved_errno = errno;
	const char *side = merged ? "entry" : NULL;
	bool first = true;

	if (merged)
		jsonl_syscall_merged_open("shmget", __NR_shmget);
	else
		jsonl_syscall_open("shmget", __NR_shmget, true);

	jsonl_arg_open(0, "key", side);
	jsonl_const(key, true, NULL, NULL);
	jsonl_arg_close(side);

	printf(", ");
	jsonl_arg_open(1, "size", side);
	jsonl_unsigned(1);
	jsonl_arg_close(side);

	printf(", ");
	jsonl_arg_open(2, "shmflg", side);
	jsonl_flags_groups_open(shmflg);
	if (flags | unknown) {
		jsonl_flags_group_open(flags | unknown, true);
		if (flags & IPC_EXCL) {
			jsonl_flags_const(IPC_EXCL, "IPC_EXCL", first);
			first = false;
		}
		if (flags & SHM_NORESERVE) {
			jsonl_flags_const(SHM_NORESERVE, "SHM_NORESERVE",
					  first);
			first = false;
		}
		if (unknown) {
			jsonl_flags_remainder(unknown, NULL, first);
			first = false;
		}
		jsonl_flags_group_close();
	}
	if (huge) {
		printf("%s{\"raw\": ", first ? "" : ", ");
		jsonl_xlat_raw(huge << SHM_HUGE_SHIFT);
		printf(", \"group_type\": \"shift\", \"elems\": [{\"type\":"
		       " \"shift\", \"raw\": ");
		jsonl_xlat_raw(huge << SHM_HUGE_SHIFT);
		printf(", \"value\": \"%u\", \"shift\": \"SHM_HUGE_SHIFT\"}]}",
		       huge);
		first = false;
	}
	mode_group(mode, first);
	jsonl_flags_groups_close();
	jsonl_arg_close(side);

	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("shmget", __NR_shmget, false);
	}
	printf("], ");
	errno = saved_errno;
	jsonl_return("decimal", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	test_shmget(0, 0, 0, 0600);
	test_shmget(IPC_EXCL | SHM_NORESERVE, 21, 0x20000, 0125);
	test_shmget(0, 21, 0, 0);
	test_shmget(0, 0, 0x20000, 0644);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_shmget")

#endif
