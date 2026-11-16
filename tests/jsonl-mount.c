/*
 * Check JSONL output of a flags value of several groups: the mountflags
 * argument of mount, whose old magic is a group of its own.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_mount

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/mount.h>

# define MGC_VAL	0xc0ed0000U

static bool merged;
static const char *side;

static const char source[] = "/nonexistent/jsonl_mount_source";
static const char target[] = "/nonexistent/jsonl_mount_target";
static const char fstype[] = "jsonl";

static void
flags_rdonly_nosuid(bool first)
{
	jsonl_flags_const(MS_RDONLY, "MS_RDONLY", first);
	jsonl_flags_const(MS_NOSUID, "MS_NOSUID", false);
}

static void
test_mount(unsigned int magic, unsigned int flags)
{
	long rc = syscall(__NR_mount, source, target, fstype,
			  (unsigned long) (magic | flags), NULL);
	int err = errno;

	if (merged)
		jsonl_syscall_merged_open("mount", __NR_mount);
	else
		jsonl_syscall_open("mount", __NR_mount, true);
	jsonl_arg_open(0, "source", side);
	jsonl_path_value(source);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "target", side);
	jsonl_path_value(target);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "filesystemtype", side);
	jsonl_string_value(fstype, sizeof(fstype) - 1, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(3, "mountflags", side);
	if (magic) {
		jsonl_flags_groups_open(magic | flags);
		jsonl_flags_group_open(magic, true);
		jsonl_flags_const(magic, "MS_MGC_VAL", true);
		jsonl_flags_group_close();
		if (flags) {
			jsonl_flags_group_open(flags, false);
			flags_rdonly_nosuid(true);
			jsonl_flags_group_close();
		}
		jsonl_flags_groups_close();
	} else {
		jsonl_flags_open(flags);
		flags_rdonly_nosuid(true);
		jsonl_flags_close();
	}
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(4, "data", side);
	jsonl_addr(NULL);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("mount", __NR_mount, false);
	}
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

	test_mount(MGC_VAL, MS_RDONLY | MS_NOSUID);
	test_mount(MGC_VAL, 0);
	test_mount(0, MS_RDONLY | MS_NOSUID);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_mount")

#endif
