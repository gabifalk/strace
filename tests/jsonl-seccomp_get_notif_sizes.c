/*
 * Check JSONL output of the seccomp SECCOMP_GET_NOTIF_SIZES operation.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"
#include "scno.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/seccomp.h>

#ifndef SECCOMP_GET_NOTIF_SIZES
# define SECCOMP_GET_NOTIF_SIZES 3
#endif

static bool merged;

static void
jsonl_seccomp_entry(unsigned int flags)
{
	const char *side = merged ? "entry" : NULL;

	jsonl_arg_open(0, "operation", side);
	printf("\"type\": \"const\", \"raw\": \"%#x\","
	       " \"sym\": \"SECCOMP_GET_NOTIF_SIZES\"",
	       SECCOMP_GET_NOTIF_SIZES);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "flags", side);
	if (flags)
		printf("\"type\": \"hex\", \"raw\": \"%#x\"", flags);
	else
		printf("\"type\": \"hex\", \"raw\": \"0\"");
	jsonl_arg_close(side);
}

static void
jsonl_seccomp(unsigned int flags, kernel_ulong_t args, const uint16_t *sizes,
	      long rc)
{
	const char *side = merged ? "exit" : NULL;

	if (merged)
		jsonl_syscall_merged_open("seccomp", __NR_seccomp);
	else
		jsonl_syscall_open("seccomp", __NR_seccomp, true);
	jsonl_seccomp_entry(flags);
	if (merged) {
		printf(", ");
	} else {
		printf("]}\n");
		jsonl_syscall_open("seccomp", __NR_seccomp, false);
	}
	jsonl_arg_open(2, "args", side);
	if (rc >= 0) {
		printf("\"type\": \"struct\", \"fields\": {"
		       "\"seccomp_notif\": {\"type\": \"unsigned\","
		       " \"raw\": \"%hu\"},"
		       " \"seccomp_notif_resp\": {\"type\": \"unsigned\","
		       " \"raw\": \"%hu\"},"
		       " \"seccomp_data\": {\"type\": \"unsigned\","
		       " \"raw\": \"%hu\"}}",
		       sizes[0], sizes[1], sizes[2]);
	} else if (args) {
		printf("\"type\": \"addr\", \"raw\": \"%#llx\"",
		       (unsigned long long) args);
	} else {
		printf("\"type\": \"addr\", \"raw\": null");
	}
	jsonl_arg_close(side);
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_ARR(uint16_t, sizes, 3);
	const kernel_ulong_t op = (kernel_ulong_t) 0xfacefeed00000000ULL
				  | SECCOMP_GET_NOTIF_SIZES;
	const kernel_ulong_t flags = (kernel_ulong_t) 0xdeadbeef00000000ULL;
	long rc;

	rc = syscall(__NR_seccomp, op, flags | 0xdeadbeef, 0);
	jsonl_seccomp(0xdeadbeef, 0, sizes, rc);

	if (F8ILL_KULONG_SUPPORTED) {
		const kernel_ulong_t bad = f8ill_ptr_to_kulong(0);
		rc = syscall(__NR_seccomp, op, flags, bad);
		jsonl_seccomp(0, bad, sizes, rc);
	}

	rc = syscall(__NR_seccomp, op, flags, (uintptr_t) (sizes + 1));
	jsonl_seccomp(0, (uintptr_t) (sizes + 1), sizes, rc);

	for (unsigned int i = 0; i < 2; i++) {
		sizes[0] = 0xcafe;
		sizes[1] = 0xfeed;
		sizes[2] = 0xbeef;
		rc = syscall(__NR_seccomp, op, flags | (i * 0xdeadc0de),
			     (uintptr_t) sizes);
		if (rc < 0 && errno != ENOSYS && errno != EINVAL
		    && errno != EPERM)
			perror_msg_and_fail("seccomp");
		jsonl_seccomp(i * 0xdeadc0de, (uintptr_t) sizes, sizes, rc);
	}

	jsonl_exit_event(0);
	return 0;
}
