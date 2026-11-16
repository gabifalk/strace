/*
 * Helpers for emitting expected JSONL output in ioctl tests.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef STRACE_TESTS_JSONL_IOCTL_H
# define STRACE_TESTS_JSONL_IOCTL_H

# include "tests.h"
# include "jsonl.h"
# include "scno.h"

# include <sys/ioctl.h>
# include <linux/ioctl.h>

/* Set by the test when it is run for the jsonl-merged mode. */
static bool jsonl_ioctl_merged;

/* Whether the third argument of the current event is printed on exiting. */
static bool jsonl_ioctl_exiting;

/* The result and the errno of the last jsonl_ioctl_call. */
static long jsonl_ioctl_rc;
static int jsonl_ioctl_errno;

/* The "dir" flags field of an ioctl op, derived from its command code. */
static inline void
jsonl_ioctl_dir(unsigned int code)
{
	const unsigned int dir = _IOC_DIR(code);
	bool first = true;

	printf(", \"dir\": {");
	jsonl_flags_open(dir);
	if (dir == _IOC_NONE) {
		jsonl_flags_const(_IOC_NONE, "_IOC_NONE", first);
	} else {
		if (dir & _IOC_READ) {
			jsonl_flags_const(_IOC_READ, "_IOC_READ", first);
			first = false;
		}
		if (dir & _IOC_WRITE)
			jsonl_flags_const(_IOC_WRITE, "_IOC_WRITE", first);
	}
	jsonl_flags_close();
	printf("}");
}

/*
 * An ioctl_op value: raw code, the names of the commands it resolves to,
 * and the _IOC parts.
 */
static inline void
jsonl_ioctl_op(unsigned int code, const char *cmd, const char *cmd2)
{
	printf("\"type\": \"ioctl_op\", \"raw\": \"%#x\"", code);
	if (cmd) {
		printf(", \"cmds\": [{\"type\": \"ioctl_cmd\","
		       " \"name\": \"%s\"}", cmd);
		if (cmd2)
			printf(", {\"type\": \"ioctl_cmd\","
			       " \"name\": \"%s\"}", cmd2);
		printf("]");
	}
	jsonl_ioctl_dir(code);
	printf(", \"ioc_type\": \"%#x\", \"ioc_nr\": \"%#x\","
	       " \"ioc_size\": \"%#x\"",
	       _IOC_TYPE(code), _IOC_NR(code), _IOC_SIZE(code));
}

/* Invoke the ioctl and open its event up to the end of the op argument. */
static inline void
jsonl_ioctl_call(unsigned long code, const char *cmd, const char *cmd2,
		 const void *arg)
{
	const char *side = jsonl_ioctl_merged ? "entry" : NULL;

	jsonl_ioctl_rc = ioctl(-1, code, arg);
	jsonl_ioctl_errno = errno;
	jsonl_ioctl_exiting = false;

	if (jsonl_ioctl_merged)
		jsonl_syscall_merged_open("ioctl", __NR_ioctl);
	else
		jsonl_syscall_open("ioctl", __NR_ioctl, true);

	jsonl_arg_open(0, "fd", side);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "op", side);
	jsonl_ioctl_op(code, cmd, cmd2);
	jsonl_arg_close(side);
}

/* The side of the third argument in the merged mode. */
static inline const char *
jsonl_ioctl_argp_side(void)
{
	if (!jsonl_ioctl_merged)
		return NULL;
	return jsonl_ioctl_exiting ? "exit" : "entry";
}

/* Open the third argument, printed on entering or on exiting. */
static inline void
jsonl_ioctl_argp_open(const char *name, bool on_exit)
{
	jsonl_ioctl_exiting = on_exit;
	if (on_exit && !jsonl_ioctl_merged) {
		printf("]}\n");
		jsonl_syscall_open("ioctl", __NR_ioctl, false);
	} else {
		printf(", ");
	}
	jsonl_arg_open(2, name, jsonl_ioctl_argp_side());
}

/* Close the third argument. */
static inline void
jsonl_ioctl_argp_close(void)
{
	jsonl_arg_close(jsonl_ioctl_argp_side());
}

/* The rest of the event: the exiting part and the return value. */
static inline void
jsonl_ioctl_end(void)
{
	if (!jsonl_ioctl_merged && !jsonl_ioctl_exiting) {
		printf("]}\n");
		jsonl_syscall_open("ioctl", __NR_ioctl, false);
	}
	printf("], ");
	errno = jsonl_ioctl_errno;
	jsonl_return("decimal", jsonl_ioctl_rc);
	printf("}\n");
}


#endif /* !STRACE_TESTS_JSONL_IOCTL_H */
