/*
 * Check JSONL output of the PTP_CLOCK_GETCAPS ioctl command: the signed
 * max_phase_adj field of struct ptp_clock_caps is a ticks value.
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
# include <linux/ptp_clock.h>

static void
emit_ticks_d(int val)
{
	printf("\"type\": \"ticks\", \"raw\": \"%d\", \"hz\": \"1000000000\","
	       " \"precision\": \"9\"", val);
}

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_PTR(struct ptp_clock_caps, caps);
	memset(caps, 0, sizeof(*caps));
	caps->max_adj = 100000;
	caps->n_pins = 2;
	caps->adjust_phase = 1;
	caps->max_phase_adj = -1000000;

	jsonl_ioctl_call(PTP_CLOCK_GETCAPS, "PTP_CLOCK_GETCAPS", NULL, caps);
	jsonl_ioctl_argp_open("argp", true);
	jsonl_struct_open();
	jsonl_field_decimal("max_adj", true, caps->max_adj);
	jsonl_field_decimal("n_alarm", false, caps->n_alarm);
	jsonl_field_decimal("n_ext_ts", false, caps->n_ext_ts);
	jsonl_field_decimal("n_per_out", false, caps->n_per_out);
	jsonl_field_decimal("pps", false, caps->pps);
	jsonl_field_decimal("n_pins", false, caps->n_pins);
	jsonl_field_decimal("cross_timestamping", false,
			    caps->cross_timestamping);
	jsonl_field_decimal("adjust_phase", false, caps->adjust_phase);
	jsonl_field_open("max_phase_adj", false);
	emit_ticks_d(caps->max_phase_adj);
	jsonl_field_close();
	jsonl_struct_close();
	jsonl_ioctl_argp_close();
	printf("], ");
	jsonl_return_injected("unsigned", jsonl_ioctl_rc);
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
