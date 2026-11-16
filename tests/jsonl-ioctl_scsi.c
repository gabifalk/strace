/*
 * Check JSONL output of the SG_SCSI_RESET ioctl command: its argument
 * is a reset type composed with the SG_SCSI_RESET_NO_ESCALATE flag.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_ioctl && defined HAVE_SCSI_SG_H

# include "jsonl_ioctl.h"

# include <stdio.h>
# include <string.h>
# include <scsi/sg.h>

# define XLAT_MACROS_ONLY
#  include "xlat/sg_scsi_reset.h"
# undef XLAT_MACROS_ONLY

static void
test_reset(int *const p, const int val)
{
	*p = val;
	jsonl_ioctl_call(SG_SCSI_RESET, "SG_SCSI_RESET", NULL, p);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_flags_open(val);
	if (val & SG_SCSI_RESET_NO_ESCALATE) {
		jsonl_flags_const(SG_SCSI_RESET_NO_ESCALATE,
				  "SG_SCSI_RESET_NO_ESCALATE", true);
		printf(", ");
	}
	printf("{");
	if ((val & ~SG_SCSI_RESET_NO_ESCALATE) == SG_SCSI_RESET_BUS)
		jsonl_const(SG_SCSI_RESET_BUS, true, "SG_SCSI_RESET_BUS",
			    NULL);
	else
		jsonl_const(val & ~SG_SCSI_RESET_NO_ESCALATE, true, NULL,
			    "SG_SCSI_RESET_???");
	printf("}");
	jsonl_flags_close();
	jsonl_indirect();
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();
}

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_PTR(int, p);

	test_reset(p, SG_SCSI_RESET_NO_ESCALATE | SG_SCSI_RESET_BUS);
	test_reset(p, SG_SCSI_RESET_BUS);
	test_reset(p, SG_SCSI_RESET_NO_ESCALATE | 0xbad0);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl && HAVE_SCSI_SG_H")

#endif
