/*
 * Check JSONL output of LIRC_* commands of ioctl syscall.
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
# include <linux/lirc.h>

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


static unsigned int *value;

static void
test_unsigned(unsigned int code, const char *cmd, const char *cmd2,
	      unsigned int val)
{
	*value = val;
	ioctl_call(code, cmd, cmd2, value);
	argp_open("argp", false);
	jsonl_unsigned(val);
	printf(", \"indirect\": true");
	argp_close();
	ioctl_end();
}

/* A NULL sym stands for a mode that is not known. */
static void
test_mode(unsigned int code, const char *cmd, unsigned int val,
	  const char *sym)
{
	*value = val;
	ioctl_call(code, cmd, NULL, value);
	argp_open("argp", false);
	jsonl_const(val, true, sym, "LIRC_MODE_???");
	printf(", \"indirect\": true");
	argp_close();
	ioctl_end();
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	value = tail_alloc(sizeof(*value));

	test_unsigned(ARG_STR(LIRC_SET_REC_TIMEOUT), NULL, 12000);
	test_unsigned(ARG_STR(LIRC_SET_WIDEBAND_RECEIVER), NULL, 1);
	test_unsigned(ARG_STR(LIRC_SET_MEASURE_CARRIER_MODE), NULL, 0);
	test_unsigned(ARG_STR(LIRC_SET_REC_TIMEOUT_REPORTS), NULL, 4294967295U);
	test_unsigned(ARG_STR(LIRC_SET_SEND_DUTY_CYCLE), NULL, 33);
	test_unsigned(ARG_STR(LIRC_SET_SEND_CARRIER), NULL, 38000);
	test_unsigned(ARG_STR(LIRC_SET_REC_CARRIER), NULL, 20000);
	test_unsigned(LIRC_SET_REC_CARRIER_RANGE,
		      "IPMICTL_SET_MAINTENANCE_MODE_CMD",
		      "LIRC_SET_REC_CARRIER_RANGE", 40000);

	test_mode(ARG_STR(LIRC_SET_SEND_MODE), ARG_STR(LIRC_MODE_PULSE));
	test_mode(ARG_STR(LIRC_SET_SEND_MODE), ARG_STR(LIRC_MODE_SCANCODE));
	test_mode(ARG_STR(LIRC_SET_REC_MODE), ARG_STR(LIRC_MODE_MODE2));
	test_mode(ARG_STR(LIRC_SET_REC_MODE), ARG_STR(LIRC_MODE_LIRCCODE));
	test_mode(ARG_STR(LIRC_SET_REC_MODE), 3, NULL);

	*value = 31;
	ioctl_call(ARG_STR(LIRC_SET_TRANSMITTER_MASK), NULL, value);
	argp_open("argp", false);
	jsonl_hex(*value);
	printf(", \"indirect\": true");
	argp_close();
	ioctl_end();

	ioctl_call(ARG_STR(LIRC_GET_FEATURES), NULL, value);
	argp_open("argp", true);
	jsonl_addr(value);
	argp_close();
	ioctl_end();

	ioctl_call(_IO('i', 0xff), NULL, NULL, value);
	argp_open("argp", false);
	jsonl_hex((uintptr_t) value);
	argp_close();
	ioctl_end();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
