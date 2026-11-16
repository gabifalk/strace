/*
 * Check JSONL output of the pidfd_send_signal syscall.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_pidfd_send_signal

# include "jsonl.h"

# include <fcntl.h>
# include <signal.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

static bool merged;

static const struct strval32 known_flags[] = {
	{ 1, "PIDFD_SIGNAL_THREAD" },
	{ 2, "PIDFD_SIGNAL_THREAD_GROUP" },
	{ 4, "PIDFD_SIGNAL_PROCESS_GROUP" },
};

/* The fields of the siginfo_t of a signal queued by a user process. */
static void
jsonl_siginfo(const siginfo_t *si, const char *signo_sym,
	      const char *code_sym)
{
	jsonl_struct_open();
	jsonl_field_open("si_signo", true);
	jsonl_signal(si->si_signo, signo_sym);
	jsonl_field_close();
	jsonl_field_open("si_code", false);
	jsonl_const((unsigned int) si->si_code, true, code_sym, NULL);
	jsonl_field_close();
	jsonl_field_open("si_errno", false);
	jsonl_unsigned((unsigned int) si->si_errno);
	jsonl_field_close();
	jsonl_field_open("si_pid", false);
	jsonl_decimal(si->si_pid);
	jsonl_field_close();
	jsonl_field_open("si_uid", false);
	printf("\"type\": \"uid\", \"raw\": \"%d\"", (int) si->si_uid);
	jsonl_field_close();
	jsonl_field_open("si_int", false);
	jsonl_decimal(si->si_int);
	jsonl_field_close();
	jsonl_field_open("si_ptr", false);
	jsonl_addr(si->si_ptr);
	jsonl_field_close();
	jsonl_struct_close();
}

/* The info argument is printed as a siginfo_t when signo_sym is set. */
static void
test_pidfd_send_signal(int pidfd, int sig, const char *sig_sym,
		       const siginfo_t *info, const char *signo_sym,
		       const char *code_sym, int flags)
{
	const char *side = merged ? "entry" : NULL;
	const kernel_ulong_t fill = (kernel_ulong_t) 0xdefaced00000000ULL;
	const kernel_ulong_t arg1 = fill | (unsigned int) pidfd;
	const kernel_ulong_t arg2 = fill | (unsigned int) sig;
	const kernel_ulong_t arg3 = (unsigned long) info;
	const kernel_ulong_t arg4 = fill | (unsigned int) flags;

	const long rc = syscall(__NR_pidfd_send_signal, arg1, arg2, arg3, arg4);
	const int saved_errno = errno;

	if (merged)
		jsonl_syscall_merged_open("pidfd_send_signal",
					  __NR_pidfd_send_signal);
	else
		jsonl_syscall_open("pidfd_send_signal",
				   __NR_pidfd_send_signal, true);

	jsonl_arg_open(0, "pidfd", side);
	jsonl_fd(pidfd, NULL, false);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(1, "signum", side);
	jsonl_signal(sig, sig_sym);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(2, "info", side);
	if (signo_sym)
		jsonl_siginfo(info, signo_sym, code_sym);
	else
		jsonl_addr(info);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(3, "flags", side);
	jsonl_flags((unsigned int) flags, known_flags, ARRAY_SIZE(known_flags),
		    NULL);
	jsonl_arg_close(side);

	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("pidfd_send_signal",
				   __NR_pidfd_send_signal, false);
	}
	printf("], ");
	errno = saved_errno;
	jsonl_return("decimal", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	static const char null_path[] = "/dev/null";

	int fd = open(null_path, O_RDONLY);
	if (fd < 0)
		perror_msg_and_fail("open: %s", null_path);

	TAIL_ALLOC_OBJECT_CONST_PTR(siginfo_t, si);
	const void *esi = (const void *) si + 1;

	test_pidfd_send_signal(fd, ARG_STR(SIGUSR1), esi, NULL, NULL, 0);

	si->si_signo = SIGUSR1;
	si->si_code = SI_QUEUE;
	si->si_pid = getpid();

	test_pidfd_send_signal(fd, ARG_STR(SIGUSR2), si, "SIGUSR1", "SI_QUEUE",
			       -1);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_pidfd_send_signal")

#endif
