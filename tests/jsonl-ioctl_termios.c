/*
 * Check JSONL output of the termios structure of the TCSETS ioctl command:
 * the composed c_oflag and c_cflag flags with the baud rate shift, the
 * missing flag bits, and the indices of the c_cc array.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_ioctl && defined __NR_openat				\
 && !(defined __sparc__ || defined __powerpc__ || defined __powerpc64__	\
      || defined __alpha__ || defined __mips__ || defined __hppa__)

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <linux/fcntl.h>
# include <linux/ioctl.h>
# include <linux/termios.h>

# include "xlat.h"
# include "xlat/termios_cc.h"

# ifndef IBSHIFT
#  define IBSHIFT 16
# endif

extern int ioctl(int __fd, unsigned long int __request, ...);

static void
print_cflag(void)
{
	const unsigned int ibaud = B1800 << IBSHIFT;

	jsonl_flags_open(B600 | ibaud | CS8 | CREAD | CLOCAL);
	jsonl_flags_const(B600, "B600", true);
	printf(", {\"type\": \"shift\", \"raw\": \"%#x\", \"value\": {",
	       ibaud);
	jsonl_const(B1800, true, "B1800", NULL);
	printf("}, \"shift\": \"IBSHIFT\"}");
	jsonl_flags_const(CS8, "CS8", false);
	jsonl_flags_const(CREAD, "CREAD", false);
	jsonl_flags_const(CLOCAL, "CLOCAL", false);
	jsonl_flags_close();
}

static void
print_oflag(void)
{
	static const char *const syms[] = {
		"NL0", "CR0", "TAB0", "BS0", "VT0", "FF0"
	};

	jsonl_flags_open(0);
	for (size_t i = 0; i < ARRAY_SIZE(syms); ++i)
		jsonl_flags_const(0, syms[i], !i);
	jsonl_flags_close();
}

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "entry" : NULL;

	int fd = syscall(__NR_openat, -100, "/dev/ptmx", O_RDWR | O_NOCTTY);
	if (fd < 0)
		perror_msg_and_skip("open: %s", "/dev/ptmx");

	TAIL_ALLOC_OBJECT_CONST_PTR(struct termios, tios);
	memset(tios, 0, sizeof(*tios));
	tios->c_cflag = B600 | (B1800 << IBSHIFT) | CS8 | CREAD | CLOCAL;
	for (unsigned int i = 0; i < NCCS; ++i)
		tios->c_cc[i] = 0x10 + i;

	long rc = ioctl(fd, TCSETS, tios);

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
	       " [{\"type\": \"ioctl_cmd\", \"name\": \"TCSETS\"}]",
	       (unsigned int) TCSETS);
	printf(", \"dir\": {");
	jsonl_flags_open(_IOC_NONE);
	jsonl_flags_const(_IOC_NONE, "_IOC_NONE", true);
	jsonl_flags_close();
	printf("}, \"ioc_type\": \"%#x\", \"ioc_nr\": \"%#x\","
	       " \"ioc_size\": \"%#x\"",
	       _IOC_TYPE(TCSETS), _IOC_NR(TCSETS), _IOC_SIZE(TCSETS));
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "argp", side);
	jsonl_struct_open();
	printf("\"c_iflag\": {");
	jsonl_flags_open_dflt_null(0);
	jsonl_flags_close();
	printf("}, \"c_oflag\": {");
	print_oflag();
	printf("}, \"c_cflag\": {");
	print_cflag();
	printf("}, \"c_lflag\": {");
	jsonl_flags_open_dflt_null(0);
	jsonl_flags_close();
	printf("}");
	jsonl_field_const("c_line", false, 0, "N_TTY", NULL);
	printf(", \"c_cc\": {");
	jsonl_array_open();
	for (unsigned int i = 0; i < NCCS; ++i) {
		printf("%s{\"index\": {", i ? ", " : "");
		jsonl_const(i, false, jsonl_xlat_sym(termios_cc, i), NULL);
		printf("}, ");
		jsonl_hex(tios->c_cc[i]);
		printf("}");
	}
	jsonl_array_close();
	printf("}");
	jsonl_struct_close();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("ioctl", __NR_ioctl, false);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl && __NR_openat")

#endif
