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

# include "jsonl_ioctl.h"

# include <stdio.h>
# include <string.h>
# include <sys/ioctl.h>
# include <linux/ioctl.h>
# include <linux/lirc.h>

static unsigned int *value;

static void
test_unsigned(unsigned int code, const char *cmd, const char *cmd2,
	      unsigned int val)
{
	*value = val;
	jsonl_ioctl_call(code, cmd, cmd2, value);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_unsigned(val);
	jsonl_indirect();
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();
}

/* A NULL sym stands for a mode that is not known. */
static void
test_mode(unsigned int code, const char *cmd, unsigned int val,
	  const char *sym)
{
	*value = val;
	jsonl_ioctl_call(code, cmd, NULL, value);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_const(val, true, sym, "LIRC_MODE_???");
	jsonl_indirect();
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();
}

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

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
	jsonl_ioctl_call(ARG_STR(LIRC_SET_TRANSMITTER_MASK), NULL, value);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_hex(*value);
	jsonl_indirect();
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_ioctl_call(ARG_STR(LIRC_GET_FEATURES), NULL, value);
	jsonl_ioctl_argp_open("argp", true);
	jsonl_addr(value);
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_ioctl_call(_IO('i', 0xff), NULL, NULL, value);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_hex((uintptr_t) value);
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
