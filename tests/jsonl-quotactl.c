/*
 * Check JSONL output of a composite command: the op argument of quotactl,
 * whose command and quota type are its two named parts.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_quotactl

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <linux/quota.h>

# define Q_UNKNOWN	0x123U
# define TYPE_UNKNOWN	5U

static bool merged;
static const char *side;

static const char special[] = "/nonexistent/jsonl_quotactl_special";
static const char qfile[] = "/nonexistent/jsonl_quotactl_file";

static void
quotactl_open(void)
{
	if (merged)
		jsonl_syscall_merged_open("quotactl", __NR_quotactl);
	else
		jsonl_syscall_open("quotactl", __NR_quotactl, true);
}

static void
print_op(unsigned int cmd, const char *cmd_sym,
	 unsigned int type, const char *type_sym)
{
	jsonl_arg_open(0, "op", side);
	printf("\"type\": \"qcmd_op\", \"raw\": \"%#x\", \"cmd\": {",
	       (unsigned int) QCMD(cmd, type));
	jsonl_const(cmd, true, cmd_sym, "Q_???");
	printf("}, \"quota_type\": {");
	jsonl_const(type, true, type_sym, "???QUOTA");
	printf("}");
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "special", side);
	jsonl_path_value(special);
	jsonl_arg_close(side);
}

static void
quotactl_close(long rc, int err)
{
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("quotactl", __NR_quotactl, false);
	}
	printf("], ");
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	long rc = syscall(__NR_quotactl, QCMD(Q_QUOTAON, USRQUOTA), special,
			  QFMT_VFS_OLD, qfile);
	int err = errno;
	quotactl_open();
	print_op(Q_QUOTAON, "Q_QUOTAON", USRQUOTA, "USRQUOTA");
	printf(", ");
	jsonl_arg_open(2, "id", side);
	jsonl_const(QFMT_VFS_OLD, true, "QFMT_VFS_OLD", NULL);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(3, "addr", side);
	jsonl_path_value(qfile);
	jsonl_arg_close(side);
	quotactl_close(rc, err);

	rc = syscall(__NR_quotactl, QCMD(Q_SYNC, TYPE_UNKNOWN), special, 0, 0);
	err = errno;
	quotactl_open();
	print_op(Q_SYNC, "Q_SYNC", TYPE_UNKNOWN, NULL);
	quotactl_close(rc, err);

	rc = syscall(__NR_quotactl, QCMD(Q_UNKNOWN, PRJQUOTA), special, 7, 0);
	err = errno;
	quotactl_open();
	print_op(Q_UNKNOWN, NULL, PRJQUOTA, "PRJQUOTA");
	printf(", ");
	jsonl_arg_open(2, "id", side);
	jsonl_unsigned(7);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(3, "addr", side);
	jsonl_addr(NULL);
	jsonl_arg_close(side);
	quotactl_close(rc, err);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_quotactl")

#endif
