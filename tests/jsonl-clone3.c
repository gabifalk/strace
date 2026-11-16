/*
 * Check JSONL output of the exiting struct clone_args of clone3: the values
 * stored through the pidfd and parent_tid pointers, and a nonzero tail,
 * emitted as the changed exiting value of the cl_args argument.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_clone3

# include "jsonl.h"

# include <signal.h>
# include <stdio.h>
# include <string.h>
# include <linux/sched.h>

# define RETVAL 42
# define STACK_SIZE 0xbadc0dedU

static bool merged;

static void
open_clone3(void)
{
	if (merged)
		jsonl_syscall_merged_open("clone3", __NR_clone3);
	else
		jsonl_syscall_open("clone3", __NR_clone3, true);
	jsonl_arg_open(0, "cl_args", merged ? "entry" : NULL);
	jsonl_struct_open();
}

static void
print_common_fields(void)
{
	jsonl_field_open("exit_signal", false);
	jsonl_signal(SIGCHLD, "SIGCHLD");
	jsonl_field_close();
	jsonl_field_open("stack", false);
	jsonl_addr(NULL);
	jsonl_field_close();
	jsonl_field_hex("stack_size", false, STACK_SIZE);
}

static void
switch_to_exit(void)
{
	if (merged) {
		printf("}, \"exit\": {");
	} else {
		printf("}]}\n");
		jsonl_syscall_open("clone3", __NR_clone3, false);
		jsonl_arg_open(0, "cl_args", NULL);
	}
	printf("\"changed\": true, ");
	jsonl_struct_open();
}

static void
close_clone3(unsigned long size)
{
	jsonl_arg_close(merged ? "exit" : NULL);
	printf(", ");
	jsonl_arg_open(1, "size", merged ? "exit" : NULL);
	jsonl_unsigned(size);
	jsonl_arg_close(merged ? "exit" : NULL);
	printf("], ");
	jsonl_return_injected("tid", RETVAL);
	printf("}\n");
}

static void
print_tail(unsigned int offset, const void *tail, unsigned int size)
{
	printf(", \"extra_data\": {\"type\": \"bytes\", \"offset\": \"%u\","
	       " \"size\": \"%u\", \"value\": ", offset, size);
	jsonl_bytes_n(tail, size);
	printf("}");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_PTR(int, pidfd);
	TAIL_ALLOC_OBJECT_CONST_PTR(int, parent_tid);
	*pidfd = 0x0badfd00;
	*parent_tid = 0x7eedbeef;

	const unsigned int size0 = offsetofend(struct clone_args, tls);
	struct clone_args *const args =
		tail_alloc(sizeof(*args) + sizeof(uint64_t));

	memset(args, 0, sizeof(*args) + sizeof(uint64_t));
	args->flags = CLONE_PIDFD | CLONE_PARENT_SETTID;
	args->pidfd = (uintptr_t) pidfd;
	args->parent_tid = (uintptr_t) parent_tid;
	args->exit_signal = SIGCHLD;
	args->stack_size = STACK_SIZE;

	syscall(__NR_clone3, args, size0);

	open_clone3();
	jsonl_field_open("flags", true);
	jsonl_flags_open(args->flags);
	jsonl_flags_const(CLONE_PIDFD, "CLONE_PIDFD", true);
	jsonl_flags_const(CLONE_PARENT_SETTID, "CLONE_PARENT_SETTID", false);
	jsonl_flags_close();
	jsonl_field_close();
	jsonl_field_open("pidfd", false);
	jsonl_addr(pidfd);
	jsonl_field_close();
	jsonl_field_open("parent_tid", false);
	jsonl_addr(parent_tid);
	jsonl_field_close();
	print_common_fields();
	jsonl_struct_close();
	switch_to_exit();
	jsonl_field_open("pidfd", true);
	printf("\"type\": \"fd\", \"raw\": \"%d\"", *pidfd);
	jsonl_indirect();
	jsonl_field_close();
	jsonl_field_open("parent_tid", false);
	jsonl_pid("tid", *parent_tid);
	jsonl_indirect();
	jsonl_field_close();
	jsonl_struct_close();
	close_clone3(size0);

	static const uint64_t tail = 0xfacecafedeadbeefULL;
	const unsigned int size1 = sizeof(*args) + sizeof(tail);

	memset(args, 0, size1);
	args->exit_signal = SIGCHLD;
	args->stack_size = STACK_SIZE;
	memcpy((char *) args + sizeof(*args), &tail, sizeof(tail));

	syscall(__NR_clone3, args, size1);

	open_clone3();
	jsonl_field_open("flags", true);
	jsonl_flags(0, NULL, 0, NULL);
	jsonl_field_close();
	print_common_fields();
	jsonl_struct_close();
	print_tail(sizeof(*args), &tail, sizeof(tail));
	switch_to_exit();
	jsonl_struct_close();
	print_tail(sizeof(*args), &tail, sizeof(tail));
	close_clone3(size1);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_clone3")

#endif
