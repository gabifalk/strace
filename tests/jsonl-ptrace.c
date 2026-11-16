/*
 * Check JSONL output of ptrace arguments decoded on exiting: the data
 * argument of PTRACE_PEEKDATA emitted on exiting only, and the split
 * struct iovec of PTRACE_SETREGSET and PTRACE_GETREGSET, including
 * the change of iov_len.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_ptrace

# include "jsonl.h"

# include <signal.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/ptrace.h>
# include <sys/uio.h>
# include <sys/wait.h>

# if defined __x86_64__ || defined __i386__
#  define TEST_NT		0x202
#  define TEST_NT_STR	"NT_X86_XSTATE"
# elif defined __aarch64__
#  define TEST_NT		0x401
#  define TEST_NT_STR	"NT_ARM_TLS"
# endif

static bool merged;

static long
do_ptrace(unsigned long request, int pid, unsigned long addr,
	  unsigned long data)
{
	return syscall(__NR_ptrace, request, pid, addr, data);
}

static void
open_ptrace(unsigned long request, const char *request_str, int pid)
{
	const char *const side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("ptrace", __NR_ptrace);
	else
		jsonl_syscall_open("ptrace", __NR_ptrace, true);
	jsonl_arg_open(0, "op", side);
	jsonl_const(request, true, request_str, NULL);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "pid", side);
	jsonl_pid("tgid", pid);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "addr", side);
}

static void
switch_to_exit(void)
{
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("ptrace", __NR_ptrace, false);
	}
}

static void
close_ptrace(long rc, int err)
{
	printf("], ");
	errno = err;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

static void
print_iovec_entering(const struct iovec *iov, bool with_base)
{
	jsonl_arg_open(3, "data", merged ? "entry" : NULL);
	jsonl_struct_open();
	if (with_base) {
		jsonl_field_open("iov_base", true);
		jsonl_addr(iov->iov_base);
		jsonl_field_close();
	}
	jsonl_field_unsigned("iov_len", !with_base, iov->iov_len);
	jsonl_struct_close();
	printf(", \"split\": true");
	if (merged)
		printf("}, \"exit\": {");
	else
		jsonl_arg_close(NULL);
}

static void
print_iovec_exiting(const struct iovec *iov, bool with_base, bool changed)
{
	if (!merged) {
		switch_to_exit();
		jsonl_arg_open(3, "data", NULL);
	}
	jsonl_struct_open();
	if (with_base) {
		jsonl_field_open("iov_base", true);
		jsonl_addr(iov->iov_base);
		jsonl_field_close();
	}
	jsonl_field_open("iov_len", !with_base);
	if (changed)
		printf("\"changed\": true, ");
	jsonl_unsigned(iov->iov_len);
	jsonl_field_close();
	jsonl_struct_close();
	printf(", \"split\": true");
	jsonl_arg_close(merged ? "exit" : NULL);
}

static void
test_peekdata(int pid, unsigned long addr, unsigned long data)
{
	long rc = do_ptrace(PTRACE_PEEKDATA, pid, addr, data);
	int err = errno;

	open_ptrace(PTRACE_PEEKDATA, "PTRACE_PEEKDATA", pid);
	jsonl_addr((void *) addr);
	jsonl_arg_close(merged ? "entry" : NULL);
	if (merged)
		printf(", ");
	else
		switch_to_exit();
	jsonl_arg_open(3, "data", merged ? "exit" : NULL);
	jsonl_addr((void *) data);
	jsonl_arg_close(merged ? "exit" : NULL);
	close_ptrace(rc, err);
}

static void
test_setregset(int pid, struct iovec *iov)
{
	long rc = do_ptrace(PTRACE_SETREGSET, pid, 3, (uintptr_t) iov);
	int err = errno;

	open_ptrace(PTRACE_SETREGSET, "PTRACE_SETREGSET", pid);
	jsonl_const(3, true, "NT_PRPSINFO", NULL);
	jsonl_arg_close(merged ? "entry" : NULL);
	printf(", ");
	print_iovec_entering(iov, true);
	print_iovec_exiting(iov, false, false);
	close_ptrace(rc, err);
}

# ifdef TEST_NT
static void
test_getregset(int pid, struct iovec *iov)
{
	const size_t len = iov->iov_len;
	long rc = do_ptrace(PTRACE_GETREGSET, pid, TEST_NT, (uintptr_t) iov);
	int err = errno;

	if (rc)
		perror_msg_and_skip("PTRACE_GETREGSET " TEST_NT_STR);
	open_ptrace(PTRACE_GETREGSET, "PTRACE_GETREGSET", pid);
	jsonl_const(TEST_NT, true, TEST_NT_STR, NULL);
	jsonl_arg_close(merged ? "entry" : NULL);
	printf(", ");
	struct iovec entering_iov = { .iov_len = len };
	print_iovec_entering(&entering_iov, false);
	print_iovec_exiting(iov, true, iov->iov_len != len);
	close_ptrace(rc, err);
}

static void
test_tracee(struct iovec *iov)
{
	pid_t pid = fork();
	if (pid < 0)
		perror_msg_and_fail("fork");

	if (!pid) {
		if (do_ptrace(PTRACE_TRACEME, 0, 0, 0))
			perror_msg_and_fail("child: PTRACE_TRACEME");
		raise(SIGSTOP);
		_exit(0);
	}

	int status;
	if (waitpid(pid, &status, 0) != pid)
		perror_msg_and_fail("waitpid");
	if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGSTOP)
		error_msg_and_fail("unexpected wait status %#x", status);

	static char buf[65536];
	iov->iov_base = buf;
	iov->iov_len = sizeof(buf);
	test_getregset(pid, iov);

	kill(pid, SIGKILL);
	waitpid(pid, &status, 0);
}
# endif /* TEST_NT */

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	const int pid = getpid();
	TAIL_ALLOC_OBJECT_CONST_PTR(struct iovec, iov);

	test_peekdata(pid, (unsigned long) 0xfacefeedUL,
		      (unsigned long) 0xbadc0dedUL);

	iov->iov_base = (void *) (unsigned long) 0xdeadbeefUL;
	iov->iov_len = 42;
	test_setregset(pid, iov);

# ifdef TEST_NT
	test_tracee(iov);
# endif

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ptrace")

#endif
