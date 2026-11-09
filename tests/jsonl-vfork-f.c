/*
 * Check JSONL output of chdir syscall events of a vforked child under -f.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"
#include "scno.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define prefix "jsonl-vfork-f."

static int
logit_(const char *const str)
{
	return !chdir(str);
}

#define logit(arg) logit_(prefix arg)

static void
jsonl_chdir(pid_t pid, const char *arg, bool merged)
{
	const char *side = merged ? "entry" : NULL;
	char path[256];

	snprintf(path, sizeof(path), "%s%s", prefix, arg);

	if (merged)
		jsonl_syscall_merged_open_pid(pid, "chdir", __NR_chdir);
	else
		jsonl_syscall_open_pid(pid, "chdir", __NR_chdir, true);
	jsonl_arg_open(0, "path", side);
	printf("\"type\": \"path\", \"value\": ");
	jsonl_bytes(path);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open_pid(pid, "chdir", __NR_chdir, false);
	}
	printf("], \"return\": {\"type\": \"decimal\", \"raw\": \"-1\","
	       " \"errno\": {\"type\": \"errno\", \"raw\": \"%d\","
	       " \"sym\": \"ENOENT\", \"strerror\": \"%s\"}}}\n",
	       ENOENT, strerror(ENOENT));
}

int
main(int ac, char **av)
{
	if (ac < 1)
		return 1;
	if (ac > 2) {
		if (read(0, &ac, sizeof(int)))
			return 2;
		return logit("exec");
	}

	const bool merged = ac > 1 && !strcmp(av[1], "merged");

	logit("start");

	int child_wait_fds[2];
	(void) close(0);
	if (pipe(child_wait_fds))
		perror_msg_and_fail("pipe");
	if (fcntl(child_wait_fds[1], F_SETFD, FD_CLOEXEC))
		perror_msg_and_fail("fcntl");

	int parent_wait_fds[2];
	if (pipe(parent_wait_fds))
		perror_msg_and_fail("pipe");
	if (fcntl(parent_wait_fds[0], F_SETFD, FD_CLOEXEC))
		perror_msg_and_fail("fcntl");
	if (fcntl(parent_wait_fds[1], F_SETFD, FD_CLOEXEC))
		perror_msg_and_fail("fcntl");

	char *const args[] = { av[0], (char *) "", (char *) "", NULL };
	pid_t pid = vfork();

	if (pid < 0)
		perror_msg_and_fail("vfork");

	if (!pid) {
		if (logit("child") || execve(args[0], args, args + 2))
			_exit(2);
	}

	close(0);
	close(parent_wait_fds[1]);

	if (read(parent_wait_fds[0], &parent_wait_fds[1], sizeof(int)))
		perror_msg_and_fail("read");
	logit("parent");
	close(child_wait_fds[1]);

	int status;
	assert(wait(&status) == pid);
	assert(status == 0);

	pid_t ppid = getpid();
	logit("finish");

	jsonl_chdir(ppid, "start", merged);
	jsonl_chdir(pid, "child", merged);
	jsonl_chdir(ppid, "parent", merged);
	jsonl_chdir(pid, "exec", merged);
	jsonl_chdir(ppid, "finish", merged);
	return 0;
}
