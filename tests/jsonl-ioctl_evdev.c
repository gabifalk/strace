/*
 * Check JSONL output of parametric ioctl commands: the op argument
 * of EVIOCGABS and EVIOCGBIT ioctl commands, whose ioctl_cmd carries
 * the arguments encoded in the command number.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_ioctl

# include "jsonl_ioctl.h"

# include <stdio.h>
# include <string.h>
# include <linux/input.h>

static void
test_ioctl(unsigned int code, const char *name)
{
	const char *const side = jsonl_ioctl_merged ? "entry" : NULL;

	jsonl_ioctl_rc = ioctl(-1, code, NULL);
	jsonl_ioctl_errno = errno;
	jsonl_ioctl_exiting = false;

	if (jsonl_ioctl_merged)
		jsonl_syscall_merged_open("ioctl", __NR_ioctl);
	else
		jsonl_syscall_open("ioctl", __NR_ioctl, true);
	jsonl_arg_open(0, "fd", side);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "op", side);
	printf("\"type\": \"ioctl_op\", \"raw\": \"%#x\", \"cmds\": "
	       "[{\"type\": \"ioctl_cmd\", \"name\": \"%s\", \"args\": [{",
	       code, name);
}

static void
end_ioctl(unsigned int code)
{
	printf("}]}]");
	jsonl_ioctl_dir(code);
	printf(", \"ioc_type\": \"%#x\", \"ioc_nr\": \"%#x\","
	       " \"ioc_size\": \"%#x\"",
	       _IOC_TYPE(code), _IOC_NR(code), _IOC_SIZE(code));
	jsonl_arg_close(jsonl_ioctl_merged ? "entry" : NULL);
	jsonl_ioctl_argp_open("argp", true);
	jsonl_addr(NULL);
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();
}

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	test_ioctl(EVIOCGABS(ABS_X), "EVIOCGABS");
	jsonl_const(ABS_X, true, "ABS_X", NULL);
	end_ioctl(EVIOCGABS(ABS_X));

	test_ioctl(EVIOCGBIT(EV_KEY, 8), "EVIOCGBIT");
	jsonl_const(EV_KEY, true, "EV_KEY", NULL);
	printf("}, {");
	jsonl_unsigned(8);
	end_ioctl(EVIOCGBIT(EV_KEY, 8));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
