/*
 * Check JSONL output of the namespaces reported by unshare with
 * --namespace=new.  The test is run with unshare returning an injected 0.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_unshare

# include "jsonl.h"

# include <limits.h>
# include <sched.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "entry" : NULL;

	const long rc = syscall(__NR_unshare, CLONE_NEWNET | CLONE_NEWUTS);

	if (merged)
		jsonl_syscall_merged_open("unshare", __NR_unshare);
	else
		jsonl_syscall_open("unshare", __NR_unshare, true);
	jsonl_arg_open(0, "flags", side);
	jsonl_flags_open(CLONE_NEWNET | CLONE_NEWUTS);
	jsonl_flags_const(CLONE_NEWUTS, "CLONE_NEWUTS", true);
	jsonl_flags_const(CLONE_NEWNET, "CLONE_NEWNET", false);
	jsonl_flags_close();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("unshare", __NR_unshare, false);
	}
	printf("], \"return\": {");
	jsonl_unsigned(rc);
	printf(", \"namespaces\": {");
	jsonl_array_open();
	static const char *const kinds[] = { "net", "uts" };
	for (unsigned int i = 0; i < ARRAY_SIZE(kinds); ++i) {
		char path[sizeof("/proc/self/ns/") + 8];
		char link[PATH_MAX];

		snprintf(path, sizeof(path), "/proc/self/ns/%s", kinds[i]);
		const ssize_t n = readlink(path, link, sizeof(link) - 1);
		if (n <= 0)
			perror_msg_and_skip("readlink: %s", path);
		link[n] = '\0';

		const char *const inode = strchr(link, '[') + 1;
		printf("%s{\"type\": \"namespace\", \"kind\": \"%s\", "
		       "\"inode\": \"%.*s\"}", i ? ", " : "", kinds[i],
		       (int) strcspn(inode, "]"), inode);
	}
	jsonl_array_close();
	printf("}, \"injected\": true}}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_unshare")

#endif
