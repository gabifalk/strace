/*
 * Check JSONL output of timespec_t and timeval_t values: the times
 * arguments of utimensat, including a UTIME_NOW timespec, and of
 * futimesat, the req argument of nanosleep, and the timeout field,
 * whose tv_nsec is signed, of IORING_REGISTER_SYNC_CANCEL argument.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_utimensat && defined __NR_futimesat && defined __NR_nanosleep \
 && defined __NR_io_uring_register

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <sys/stat.h>
# include <sys/time.h>
# include <time.h>
# include <unistd.h>
# include <linux/io_uring.h>

static bool merged;
static const char *side;

static void
open_event(const char *name, long scno)
{
	if (merged)
		jsonl_syscall_merged_open(name, scno);
	else
		jsonl_syscall_open(name, scno, true);
	jsonl_arg_open(0, "dirfd", side);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "pathname", side);
	jsonl_addr(NULL);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "times", side);
	jsonl_array_open();
}

static void
close_event(const char *name, long scno, long rc)
{
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open(name, scno, false);
	}
	printf("], ");
	jsonl_return("decimal", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	const struct timespec ts[2] = { { 1, 2 }, { 3, UTIME_NOW } };
	long rc = syscall(__NR_utimensat, -1, NULL, ts, 0);
	open_event("utimensat", __NR_utimensat);
	printf("{");
	jsonl_timespec(1, 2);
	printf("}, {");
	jsonl_timespec(3, UTIME_NOW);
	printf(", \"sym\": \"UTIME_NOW\"}");
	jsonl_array_close();
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(3, "flags", side);
	jsonl_flags_open(0);
	jsonl_flags_close();
	jsonl_arg_close(side);
	close_event("utimensat", __NR_utimensat, rc);

	const struct timeval tv[2] = { { 4, 5 }, { 6, 7 } };
	rc = syscall(__NR_futimesat, -1, NULL, tv);
	open_event("futimesat", __NR_futimesat);
	printf("{");
	jsonl_timeval(4, 5);
	printf("}, {");
	jsonl_timeval(6, 7);
	printf("}");
	jsonl_array_close();
	jsonl_arg_close(side);
	close_event("futimesat", __NR_futimesat, rc);

	const struct timespec req = { 0, 1 };
	rc = syscall(__NR_nanosleep, &req, NULL);
	if (merged)
		jsonl_syscall_merged_open("nanosleep", __NR_nanosleep);
	else
		jsonl_syscall_open("nanosleep", __NR_nanosleep, true);
	jsonl_arg_open(0, "req", side);
	jsonl_timespec(0, 1);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("nanosleep", __NR_nanosleep, false);
	} else {
		printf(", ");
	}
	jsonl_arg_open(1, "rem", merged ? "exit" : NULL);
	jsonl_addr(NULL);
	jsonl_arg_close(merged ? "exit" : NULL);
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");

	struct io_uring_sync_cancel_reg arg;
	memset(&arg, 0, sizeof(arg));
	arg.fd = -1;
	arg.timeout.tv_sec = 1;
	arg.timeout.tv_nsec = -2;
	rc = syscall(__NR_io_uring_register, -1, IORING_REGISTER_SYNC_CANCEL,
		     &arg, 1);
	if (merged)
		jsonl_syscall_merged_open("io_uring_register",
					  __NR_io_uring_register);
	else
		jsonl_syscall_open("io_uring_register",
				   __NR_io_uring_register, true);
	jsonl_arg_open(0, "fd", side);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "opcode", side);
	jsonl_const(IORING_REGISTER_SYNC_CANCEL, true,
		    "IORING_REGISTER_SYNC_CANCEL", NULL);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "arg", side);
	jsonl_struct_open();
	printf("\"addr\": {\"type\": \"addr\", \"raw\": null}");
	printf(", \"fd\": {");
	jsonl_fd(-1, NULL, false);
	printf("}, \"flags\": {");
	jsonl_flags_open(0);
	jsonl_flags_close();
	printf("}, \"timeout\": {");
	jsonl_timespec(1, -2);
	printf("}, \"opcode\": {");
	jsonl_const(IORING_OP_NOP, false, "IORING_OP_NOP", NULL);
	printf("}");
	jsonl_struct_close();
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(3, "nr_args", side);
	jsonl_unsigned(1);
	jsonl_arg_close(side);
	close_event("io_uring_register", __NR_io_uring_register, rc);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_utimensat && __NR_futimesat && __NR_nanosleep"
		    " && __NR_io_uring_register")

#endif
