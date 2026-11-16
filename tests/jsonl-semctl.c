/*
 * Check JSONL output of the semctl syscall: the argument decoded
 * on exiting is emitted on the exiting event only.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_semctl

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <sys/sem.h>

static bool merged;

static void
semctl_open(int semid, unsigned int op, const char *op_sym)
{
	const char *side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("semctl", __NR_semctl);
	else
		jsonl_syscall_open("semctl", __NR_semctl, true);

	jsonl_arg_open(0, "semid", side);
	jsonl_decimal(semid);
	jsonl_arg_close(side);

	printf(", ");
	jsonl_arg_open(1, "semnum", side);
	jsonl_decimal(0);
	jsonl_arg_close(side);

	printf(", ");
	jsonl_arg_open(2, "op", side);
	jsonl_const(op, true, op_sym, NULL);
	jsonl_arg_close(side);

	if (merged) {
		printf(", ");
	} else {
		printf("]}\n");
		jsonl_syscall_open("semctl", __NR_semctl, false);
	}
	jsonl_arg_open(3, "arg", merged ? "exit" : NULL);
}

static void
semctl_close(long rc, int saved_errno)
{
	jsonl_arg_close(merged ? "exit" : NULL);
	printf("], ");
	errno = saved_errno;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_PTR(struct seminfo, info);
	long rc = syscall(__NR_semctl, 0, 0, IPC_INFO, info);
	int saved_errno = errno;
	if (rc < 0)
		perror_msg_and_skip("semctl IPC_INFO");

	semctl_open(0, IPC_INFO, "IPC_INFO");
	jsonl_struct_open();
	jsonl_field_decimal("semmap", true, info->semmap);
	jsonl_field_decimal("semmni", false, info->semmni);
	jsonl_field_decimal("semmns", false, info->semmns);
	jsonl_field_decimal("semmnu", false, info->semmnu);
	jsonl_field_decimal("semmsl", false, info->semmsl);
	jsonl_field_decimal("semopm", false, info->semopm);
	jsonl_field_decimal("semume", false, info->semume);
	jsonl_field_decimal("semusz", false, info->semusz);
	jsonl_field_decimal("semvmx", false, info->semvmx);
	jsonl_field_decimal("semaem", false, info->semaem);
	jsonl_struct_close();
	semctl_close(rc, saved_errno);

	TAIL_ALLOC_OBJECT_CONST_PTR(struct semid_ds, ds);
	rc = syscall(__NR_semctl, -1, 0, IPC_STAT, ds);
	saved_errno = errno;
	semctl_open(-1, IPC_STAT, "IPC_STAT");
	jsonl_addr(ds);
	semctl_close(rc, saved_errno);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_semctl")

#endif
