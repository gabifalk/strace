/*
 * Check JSONL output of the superseded event: the thread group leader
 * is superseded by an execve of another thread.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"
#include "jsonl.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void *
thread(void *arg)
{
	usleep(100000);

	printf("{\"event\": \"superseded\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, "
	       "\"old_pid\": {\"type\": \"tid\", \"raw\": \"%d\"}}\n",
	       (int) getpid(), (int) syscall(__NR_gettid));

	char *argv[] = { ((char **) arg)[0], (char *) "0", NULL };
	execve(argv[0], argv, NULL);
	perror_msg_and_fail("execve");
}

int
main(int argc, char *argv[])
{
	if (argc > 1 && !strcmp(argv[1], "0"))
		return 0;

	setvbuf(stdout, NULL, _IONBF, 0);

	pthread_t t;
	errno = pthread_create(&t, NULL, thread, argv);
	if (errno)
		perror_msg_and_fail("pthread_create");

	pause();

	return 1;
}
