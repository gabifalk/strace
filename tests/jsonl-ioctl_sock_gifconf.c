/*
 * Check JSONL output of the struct ifconf of the SIOCGIFCONF ioctl command:
 * the struct printed on exiting only when ifc_buf is NULL, the entering
 * and exiting halves of the split struct otherwise, ifc_len values, and
 * the change of ifc_len.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_ioctl

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <net/if.h>
# include <linux/ioctl.h>
# include <sys/ioctl.h>
# include <sys/socket.h>

static bool merged;

static void
print_ifc_len(int len)
{
	printf("\"type\": \"ifc_len\", \"raw\": \"%d\"", len);
	if (len > 0 && len % sizeof(struct ifreq) == 0)
		printf(", \"ifreq_count\": \"%u\"",
		       (unsigned int) (len / sizeof(struct ifreq)));
}

static void
open_ioctl(int fd)
{
	const char *const side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("ioctl", __NR_ioctl);
	else
		jsonl_syscall_open("ioctl", __NR_ioctl, true);
	jsonl_arg_open(0, "fd", side);
	jsonl_fd(fd, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "op", side);
	printf("\"type\": \"ioctl_op\", \"raw\": \"%#x\", \"cmds\":"
	       " [{\"type\": \"ioctl_cmd\", \"name\": \"SIOCGIFCONF\"}]",
	       (unsigned int) SIOCGIFCONF);
	printf(", \"dir\": {");
	jsonl_flags_open(_IOC_NONE);
	jsonl_flags_const(_IOC_NONE, "_IOC_NONE", true);
	jsonl_flags_close();
	printf("}, \"ioc_type\": \"%#x\", \"ioc_nr\": \"%#x\","
	       " \"ioc_size\": \"%#x\"",
	       _IOC_TYPE(SIOCGIFCONF), _IOC_NR(SIOCGIFCONF),
	       _IOC_SIZE(SIOCGIFCONF));
	jsonl_arg_close(side);
}

static void
open_entering_argp(void)
{
	printf(", ");
	jsonl_arg_open(2, "argp", merged ? "entry" : NULL);
	jsonl_struct_open();
}

static void
open_exiting_argp(void)
{
	if (merged) {
		printf(", ");
		jsonl_arg_open(2, "argp", "exit");
	} else {
		printf("]}\n");
		jsonl_syscall_open("ioctl", __NR_ioctl, false);
		jsonl_arg_open(2, "argp", NULL);
	}
	jsonl_struct_open();
}

static void
switch_to_exit(void)
{
	printf("}, \"split\": true");
	if (merged) {
		printf("}, \"exit\": {");
		jsonl_struct_open();
	} else {
		printf("}");
		open_exiting_argp();
	}
}

static void
close_ioctl(long rc, bool split)
{
	printf("}");
	if (split)
		printf(", \"split\": true");
	jsonl_arg_close(merged ? "exit" : NULL);
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		perror_msg_and_skip("socket AF_INET");

	TAIL_ALLOC_OBJECT_CONST_PTR(struct ifconf, ifc);
	TAIL_ALLOC_OBJECT_CONST_PTR(struct ifreq, ifr);

	ifc->ifc_len = 0;
	ifc->ifc_buf = NULL;
	long rc = ioctl(fd, SIOCGIFCONF, ifc);
	if (rc)
		perror_msg_and_skip("ioctl SIOCGIFCONF");
	open_ioctl(fd);
	open_exiting_argp();
	printf("\"ifc_len\": {");
	print_ifc_len(ifc->ifc_len);
	printf("}, \"ifc_buf\": {");
	jsonl_addr(NULL);
	printf("}");
	close_ioctl(rc, false);

	ifc->ifc_len = 1;
	ifc->ifc_buf = (void *) ifr;
	rc = ioctl(fd, SIOCGIFCONF, ifc);
	open_ioctl(fd);
	open_entering_argp();
	printf("\"ifc_len\": {");
	print_ifc_len(1);
	printf("}");
	switch_to_exit();
	printf("\"ifc_len\": {");
	if (ifc->ifc_len != 1)
		printf("\"changed\": true, ");
	print_ifc_len(ifc->ifc_len);
	printf("}, \"ifc_buf\": {");
	jsonl_array_open();
	jsonl_array_close();
	printf("}");
	close_ioctl(rc, true);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
