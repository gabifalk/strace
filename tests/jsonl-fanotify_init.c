/*
 * Check JSONL output of the flags argument of fanotify_init, a flags value
 * with a group for the notification class and a group for the flag bits.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"
#include "jsonl.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static bool merged;

static void
test_fanotify_init(const unsigned int class, const char *const class_str,
		   const unsigned int flag, const char *const flag_str)
{
	const char *const side = merged ? "entry" : NULL;
	const long rc = syscall(__NR_fanotify_init, class | flag, O_RDONLY);
	const int err = errno;

	if (rc >= 0)
		close(rc);

	if (merged)
		jsonl_syscall_merged_open("fanotify_init", __NR_fanotify_init);
	else
		jsonl_syscall_open("fanotify_init", __NR_fanotify_init, true);
	jsonl_arg_open(0, "flags", side);
	jsonl_flags_groups_open(class | flag);
	jsonl_flags_group_open(class, true);
	jsonl_flags_const(class, class_str, true);
	jsonl_flags_group_close();
	if (flag) {
		jsonl_flags_group_open(flag, false);
		jsonl_flags_const(flag, flag_str, true);
		jsonl_flags_group_close();
	}
	jsonl_flags_groups_close();
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "event_f_flags", side);
	jsonl_const(O_RDONLY, true, "O_RDONLY", NULL);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("fanotify_init", __NR_fanotify_init, false);
	}
	printf("], ");
	errno = err;
	jsonl_return("fd", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	test_fanotify_init(0, "FAN_CLASS_NOTIF", 0, NULL);
	test_fanotify_init(0x4, "FAN_CLASS_CONTENT", 0x2, "FAN_NONBLOCK");

	jsonl_exit_event(0);
	return 0;
}
