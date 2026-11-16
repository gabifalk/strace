/*
 * Check JSONL output of composite clock ids: the clockid argument
 * of clock_gettime that encodes a descriptor, a CPU clock of a process
 * or a thread, or an auxiliary clock.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_clock_gettime

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <time.h>
# include <unistd.h>

# define CLOCKFD		3
# define CPUCLOCK_SCHED		2
# define CPUCLOCK_PERTHREAD	4
# define AUX_CLOCK		16

# define TEST_FD		999
# define TEST_PID		0x7ffffff

# define MAKE_CLOCKID(id, low) \
	((clockid_t) (~(unsigned int) (id) << 3 | (low)))
# define FD_TO_CLOCKID(fd)	MAKE_CLOCKID((fd), CLOCKFD)

static bool merged;
static const char *side;
static long rc;
static int err;

static void
begin_clock_gettime(clockid_t clockid)
{
	rc = syscall(__NR_clock_gettime, clockid, NULL);
	err = errno;

	if (merged)
		jsonl_syscall_merged_open("clock_gettime", __NR_clock_gettime);
	else
		jsonl_syscall_open("clock_gettime", __NR_clock_gettime, true);
	jsonl_arg_open(0, "clockid", side);
}

static void
end_clock_gettime(void)
{
	const char *const exit_side = merged ? "exit" : NULL;

	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("clock_gettime", __NR_clock_gettime, false);
	} else {
		printf(", ");
	}
	jsonl_arg_open(1, "tp", exit_side);
	jsonl_addr(NULL);
	jsonl_arg_close(exit_side);
	printf("], ");
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");
}

static void
test_cpuclock(bool per_thread)
{
	const clockid_t clockid =
		MAKE_CLOCKID(TEST_PID, CPUCLOCK_SCHED |
				       (per_thread ? CPUCLOCK_PERTHREAD : 0));

	begin_clock_gettime(clockid);
	printf("\"type\": \"clockid\", \"raw\": \"%d\", \"kind\": \"cpu\", "
	       "\"pid\": {", (int) clockid);
	jsonl_pid(per_thread ? "tid" : "tgid", TEST_PID);
	printf("}, \"per_thread\": %s, \"cpu_clock\": {",
	       per_thread ? "true" : "false");
	jsonl_const(CPUCLOCK_SCHED, true, "CPUCLOCK_SCHED", NULL);
	printf("}");
	end_clock_gettime();
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	begin_clock_gettime(FD_TO_CLOCKID(TEST_FD));
	printf("\"type\": \"clockid\", \"raw\": \"%d\", \"kind\": \"fd\","
	       " \"fd\": \"%d\"", (int) FD_TO_CLOCKID(TEST_FD), TEST_FD);
	end_clock_gettime();

	test_cpuclock(false);
	test_cpuclock(true);

	begin_clock_gettime(AUX_CLOCK);
	printf("\"type\": \"clockid\", \"raw\": \"%#x\", \"kind\": \"aux\","
	       " \"aux_index\": \"0\"", AUX_CLOCK);
	end_clock_gettime();

	begin_clock_gettime(AUX_CLOCK + 7);
	printf("\"type\": \"clockid\", \"raw\": \"%#x\", \"kind\": \"aux\","
	       " \"aux_index\": \"7\"", AUX_CLOCK + 7);
	end_clock_gettime();

	begin_clock_gettime(CLOCK_MONOTONIC);
	jsonl_const(CLOCK_MONOTONIC, true, "CLOCK_MONOTONIC", NULL);
	end_clock_gettime();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_clock_gettime")

#endif
