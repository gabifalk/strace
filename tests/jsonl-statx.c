/*
 * Check JSONL output of a flags value of several groups: the flags
 * argument of statx, whose synchronization type is a group of its own.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_statx

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>

# define F_SYMLINK_NOFOLLOW	0x100U
# define F_EMPTY_PATH		0x1000U
# define F_FORCE_SYNC		0x2000U
# define F_DONT_SYNC		0x4000U
# define M_TYPE			0x1U
# define DIRFD			9

static bool merged;
static const char *side;

static void
test_statx(unsigned int sync_type, const char *sync_sym, unsigned int flags)
{
	const char *const exit_side = merged ? "exit" : NULL;
	long rc = syscall(__NR_statx, DIRFD, "", sync_type | flags, M_TYPE,
			  NULL);
	int err = errno;

	if (merged)
		jsonl_syscall_merged_open("statx", __NR_statx);
	else
		jsonl_syscall_open("statx", __NR_statx, true);
	jsonl_arg_open(0, "dirfd", side);
	jsonl_fd(DIRFD, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "pathname", side);
	jsonl_path_value("");
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "flags", side);
	jsonl_flags_groups_open(sync_type | flags);
	jsonl_flags_group_open(sync_type, true);
	jsonl_flags_const(sync_type, sync_sym, true);
	jsonl_flags_group_close();
	if (flags) {
		bool first = true;

		jsonl_flags_group_open(flags, false);
		if (flags & F_SYMLINK_NOFOLLOW) {
			jsonl_flags_const(F_SYMLINK_NOFOLLOW,
					  "AT_SYMLINK_NOFOLLOW", first);
			first = false;
		}
		if (flags & F_EMPTY_PATH)
			jsonl_flags_const(F_EMPTY_PATH, "AT_EMPTY_PATH", first);
		jsonl_flags_group_close();
	}
	jsonl_flags_groups_close();
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(3, "mask", side);
	jsonl_flags_open(M_TYPE);
	jsonl_flags_const(M_TYPE, "STATX_TYPE", true);
	jsonl_flags_close();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("statx", __NR_statx, false);
	} else {
		printf(", ");
	}
	jsonl_arg_open(4, "statxbuf", exit_side);
	jsonl_addr(NULL);
	jsonl_arg_close(exit_side);
	printf("], ");
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	(void) close(DIRFD);

	test_statx(0, "AT_STATX_SYNC_AS_STAT", F_EMPTY_PATH);
	test_statx(F_FORCE_SYNC, "AT_STATX_FORCE_SYNC", 0);
	test_statx(F_DONT_SYNC, "AT_STATX_DONT_SYNC",
		   F_SYMLINK_NOFOLLOW | F_EMPTY_PATH);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_statx")

#endif
