/*
 * Check JSONL output of a composed flags value: the flags argument of mmap,
 * whose map type and flag bits are a single group of elements.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_mmap2
# define TEST_NR	__NR_mmap2
# define TEST_NAME	"mmap2"
#elif defined __NR_mmap && !defined __s390x__
# define TEST_NR	__NR_mmap
# define TEST_NAME	"mmap"
#endif

#ifdef TEST_NR

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/mman.h>

# define LENGTH	12345

static bool merged;
static const char *side;

static void
test_mmap(void *addr, unsigned int prot, unsigned int type,
	  const char *type_sym, unsigned int flag, const char *flag_sym)
{
	long rc = syscall(TEST_NR, addr, LENGTH, prot, type | flag, -1, 0);
	int err = errno;

	if (merged)
		jsonl_syscall_merged_open(TEST_NAME, TEST_NR);
	else
		jsonl_syscall_open(TEST_NAME, TEST_NR, true);
	jsonl_arg_open(0, "addr", side);
	jsonl_addr(addr);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "length", side);
	jsonl_unsigned(LENGTH);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "prot", side);
	jsonl_flags_open(prot);
	jsonl_flags_const(PROT_READ, "PROT_READ", true);
	if (prot & PROT_WRITE)
		jsonl_flags_const(PROT_WRITE, "PROT_WRITE", false);
	jsonl_flags_close();
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(3, "flags", side);
	jsonl_flags_open(type | flag);
	jsonl_flags_const(type, type_sym, true);
	jsonl_flags_const(flag, flag_sym, false);
	jsonl_flags_close();
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(4, "fd", side);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(5, "offset", side);
	jsonl_hex(0);
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open(TEST_NAME, TEST_NR, false);
	}
	printf("], ");
	if (rc == -1) {
		errno = err;
		jsonl_return("hex", rc);
	} else {
		printf("\"return\": {\"type\": \"hex\", \"raw\": \"%#lx\"}",
		       (unsigned long) rc);
	}
	printf("}\n");

	if (rc != -1)
		munmap((void *) rc, LENGTH);
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	test_mmap(NULL, PROT_READ | PROT_WRITE, ARG_STR(MAP_PRIVATE),
		  ARG_STR(MAP_ANONYMOUS));
	test_mmap((void *) (uintptr_t) get_page_size(), PROT_READ,
		  ARG_STR(MAP_SHARED), ARG_STR(MAP_FIXED));

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_mmap2 || __NR_mmap")

#endif
