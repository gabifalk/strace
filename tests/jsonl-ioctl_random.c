/*
 * Check JSONL output of RND* commands of ioctl syscall.
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

# include <stdio.h>
# include <string.h>
# include <sys/ioctl.h>
# include <linux/ioctl.h>
# include <linux/types.h>
# include <linux/random.h>

# define XLAT_MACROS_ONLY
# include "xlat/random_ioctl_cmds.h"
# undef XLAT_MACROS_ONLY

static bool merged;
static bool exiting;
static long ioctl_rc;
static int ioctl_errno;

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

/*
 * An ioctl_op value: raw code, the names of the commands it resolves to,
 * and the _IOC parts.
 */
static void
emit_op(unsigned int code, const char *cmd, const char *cmd2)
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
	emit_dir(code);
	printf(", \"ioc_type\": \"%#x\", \"ioc_nr\": \"%#x\","
	       " \"ioc_size\": \"%#x\"",
	       _IOC_TYPE(code), _IOC_NR(code), _IOC_SIZE(code));
}

/* Invoke the ioctl and open its event up to the end of the op argument. */
static void
ioctl_call(unsigned long code, const char *cmd, const char *cmd2,
	   const void *arg)
{
	const char *side = merged ? "entry" : NULL;

	ioctl_rc = ioctl(-1, code, arg);
	ioctl_errno = errno;
	exiting = false;

	if (merged)
		jsonl_syscall_merged_open("ioctl", __NR_ioctl);
	else
		jsonl_syscall_open("ioctl", __NR_ioctl, true);

	jsonl_arg_open(0, "fd", side);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "op", side);
	emit_op(code, cmd, cmd2);
	jsonl_arg_close(side);
}

static const char *
argp_side(void)
{
	return merged ? (exiting ? "exit" : "entry") : NULL;
}

/* Open the third argument, printed on entering or on exiting. */
static void
argp_open(const char *name, bool on_exit)
{
	exiting = on_exit;
	if (on_exit && !merged) {
		printf("]}\n");
		jsonl_syscall_open("ioctl", __NR_ioctl, false);
	} else {
		printf(", ");
	}
	jsonl_arg_open(2, name, argp_side());
}

static void
argp_close(void)
{
	jsonl_arg_close(argp_side());
}

static void
ioctl_end(void)
{
	if (!merged && !exiting) {
		printf("]}\n");
		jsonl_syscall_open("ioctl", __NR_ioctl, false);
	}
	printf("], ");
	errno = ioctl_errno;
	jsonl_return("decimal", ioctl_rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	union {
		char c[sizeof(struct rand_pool_info) + 8];
		struct rand_pool_info info;
	} u;
	struct rand_pool_info *info = &u.info;
	int cnt = 6;

	memcpy(info->buf, "12345678", 8);
	info->buf_size = 8;
	info->entropy_count = 3;

	ioctl_call(ARG_STR(RNDGETENTCNT), NULL, &cnt);
	argp_open("argp", true);
	jsonl_addr(&cnt);
	argp_close();
	ioctl_end();

	ioctl_call(ARG_STR(RNDADDTOENTCNT), NULL, &cnt);
	argp_open("argp", false);
	jsonl_decimal(cnt);
	printf(", \"indirect\": true");
	argp_close();
	ioctl_end();

	ioctl_call(ARG_STR(RNDADDENTROPY), NULL, NULL);
	argp_open("argp", false);
	jsonl_addr(NULL);
	argp_close();
	ioctl_end();

	ioctl_call(ARG_STR(RNDADDENTROPY), NULL, info);
	argp_open("argp", false);
	jsonl_struct_open();
	jsonl_field_open("entropy_count", true);
	jsonl_decimal(info->entropy_count);
	jsonl_field_close();
	jsonl_field_open("buf_size", false);
	jsonl_decimal(info->buf_size);
	jsonl_field_close();
	jsonl_field_open("buf", false);
	jsonl_string_value(info->buf, info->buf_size, false);
	jsonl_field_close();
	jsonl_struct_close();
	argp_close();
	ioctl_end();

	ioctl_call(RNDZAPENTCNT, "FASTRPC_IOCTL_INIT_ATTACH", "RNDZAPENTCNT",
		   NULL);
	ioctl_end();

	ioctl_call(ARG_STR(RNDCLEARPOOL), NULL, NULL);
	ioctl_end();

	ioctl_call(ARG_STR(RNDRESEEDCRNG), NULL, NULL);
	ioctl_end();

	ioctl_call(_IO('R', 0xff), NULL, NULL, NULL);
	argp_open("argp", false);
	jsonl_hex(0);
	argp_close();
	ioctl_end();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
