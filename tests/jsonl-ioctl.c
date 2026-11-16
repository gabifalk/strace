/*
 * Check JSONL output of the ioctl command number (ioctl_op) argument.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_ioctl

# include "jsonl.h"

# include <linux/ioctl.h>

# ifndef INOTIFY_IOC_SETNEXTWD
#  define INOTIFY_IOC_SETNEXTWD	_IOW('I', 0, int32_t)
# endif

static bool merged;
static const char *side;

static const kernel_ulong_t magic =
	(kernel_ulong_t) 0xdeadbeefbadc0dedULL;

static long
sys_ioctl(kernel_ulong_t cmd)
{
	return syscall(__NR_ioctl, (kernel_long_t) -1, cmd, magic);
}

/* The "dir" flags field of an ioctl op, derived from its command code. */
static void
emit_dir(unsigned int code)
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

/* An ioctl_op value: raw code, optional resolved command name, and the
 * _IOC decomposition (direction flags, type, number, and size). */
static void
emit_op(unsigned int code, const char *cmd)
{
	printf("\"type\": \"ioctl_op\", \"raw\": \"%#x\"", code);
	if (cmd)
		printf(", \"cmds\": [{\"type\": \"ioctl_cmd\","
		       " \"name\": \"%s\"}]", cmd);
	emit_dir(code);
	printf(", \"ioc_type\": \"%#x\", \"ioc_nr\": \"%#x\","
	       " \"ioc_size\": \"%#x\"",
	       _IOC_TYPE(code), _IOC_NR(code), _IOC_SIZE(code));
}

static void
argp_hex(void)
{
	printf("\"type\": \"hex\", \"raw\": \"%#llx\"",
	       (unsigned long long) magic);
}

static void
argp_decimal(void)
{
	printf("\"type\": \"decimal\", \"raw\": \"%d\"", (int) magic);
}

static void
emit(unsigned int code, const char *cmd, void (*argp)(void), int err)
{
	if (merged)
		jsonl_syscall_merged_open("ioctl", __NR_ioctl);
	else
		jsonl_syscall_open("ioctl", __NR_ioctl, true);

	jsonl_arg_open(0, "fd", side);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "op", side);
	emit_op(code, cmd);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "argp", side);
	argp();
	jsonl_arg_close(side);

	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("ioctl", __NR_ioctl, false);
	}
	printf("], ");
	errno = err;
	jsonl_return("decimal", -1);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	/* An unknown command: decomposed into its _IOC parts, no cmds. */
	const unsigned int unknown = (unsigned int) 0xfeed49ed;
	sys_ioctl(unknown);
	int err_unknown = errno;

	/* A command number adjacent to a known one: still unresolved. */
	const unsigned int adjacent = INOTIFY_IOC_SETNEXTWD + 1;
	sys_ioctl(adjacent);
	int err_adjacent = errno;

	/* A resolved command: carried in the cmds array. */
	sys_ioctl(INOTIFY_IOC_SETNEXTWD);
	int err_known = errno;

	emit(unknown, NULL, argp_hex, err_unknown);
	emit(adjacent, NULL, argp_hex, err_adjacent);
	emit(INOTIFY_IOC_SETNEXTWD, "INOTIFY_IOC_SETNEXTWD", argp_decimal,
	     err_known);
	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
