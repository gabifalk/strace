/*
 * Check JSONL output of the rt_tgsigqueueinfo syscall.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_rt_tgsigqueueinfo

# include "jsonl.h"

# include <signal.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

static long
k_tgsigqueueinfo(const pid_t tgid, const int tid, const int sig,
		 const void *const info)
{
	return syscall(__NR_rt_tgsigqueueinfo,
		       F8ILL_KULONG_MASK | tgid,
		       F8ILL_KULONG_MASK | tid,
		       F8ILL_KULONG_MASK | sig,
		       info);
}

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *side = merged ? "entry" : NULL;

	const struct sigaction sa = {
		.sa_handler = SIG_IGN
	};
	if (sigaction(SIGUSR1, &sa, NULL))
		perror_msg_and_fail("sigaction");

	TAIL_ALLOC_OBJECT_CONST_PTR(siginfo_t, info);
	memset(info, 0, sizeof(*info));
	info->si_signo = SIGUSR1;
	info->si_errno = ENOENT;
	info->si_code = SI_QUEUE;
	info->si_pid = getpid();
	info->si_uid = getuid();
	info->si_value.sival_ptr =
		(void *) (unsigned long) 0xdeadbeeffacefeedULL;

	const pid_t tgid = getpid();
	const int tid = syscall(__NR_gettid);

	const long rc = k_tgsigqueueinfo(tgid, tid, SIGUSR1, info);
	if (rc)
		(errno == ENOSYS ? perror_msg_and_skip : perror_msg_and_fail)(
			"rt_tgsigqueueinfo");

	if (merged)
		jsonl_syscall_merged_open("rt_tgsigqueueinfo",
					  __NR_rt_tgsigqueueinfo);
	else
		jsonl_syscall_open("rt_tgsigqueueinfo",
				   __NR_rt_tgsigqueueinfo, true);

	jsonl_arg_open(0, "tgid", side);
	jsonl_pid("tgid", tgid);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(1, "tid", side);
	jsonl_pid("tid", tid);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(2, "signum", side);
	jsonl_signal(ARG_STR(SIGUSR1));
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(3, "info", side);
	jsonl_struct_open();
	jsonl_field_open("si_signo", true);
	jsonl_signal(ARG_STR(SIGUSR1));
	jsonl_field_close();
	jsonl_field_const("si_code", false, (unsigned int) SI_QUEUE, "SI_QUEUE",
			  NULL);
	jsonl_field_open("si_errno", false);
	jsonl_errno(ENOENT);
	jsonl_field_close();
	jsonl_field_pid("si_pid", false, "tgid", info->si_pid);
	jsonl_field_open("si_uid", false);
	printf("\"type\": \"uid\", \"raw\": \"%d\"", (int) info->si_uid);
	jsonl_field_close();
	jsonl_field_decimal("si_int", false, info->si_value.sival_int);
	jsonl_field_open("si_ptr", false);
	jsonl_addr(info->si_value.sival_ptr);
	jsonl_field_close();
	jsonl_struct_close();
	jsonl_arg_close(side);

	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("rt_tgsigqueueinfo",
				   __NR_rt_tgsigqueueinfo, false);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_rt_tgsigqueueinfo")

#endif
