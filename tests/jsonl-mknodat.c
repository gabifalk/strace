/*
 * Check JSONL output of file modes: the symbolic mode argument of mknodat
 * and the numeric mode argument and return value of umask.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_mknodat && defined __NR_umask

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/stat.h>

static bool merged;
static const char *side;

static const char path[] = "jsonl_mknodat_sample";

static void
open_event(const char *name, long scno)
{
	if (merged)
		jsonl_syscall_merged_open(name, scno);
	else
		jsonl_syscall_open(name, scno, true);
}

static void
close_event(const char *name, long scno)
{
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open(name, scno, false);
	}
	printf("], ");
}

static void
print_special(unsigned int mode)
{
	bool first = true;

	printf(", \"special\": [");
	if (mode & S_ISUID) {
		printf("\"S_ISUID\"");
		first = false;
	}
	if (mode & S_ISGID) {
		printf("%s\"S_ISGID\"", first ? "" : ", ");
		first = false;
	}
	if (mode & S_ISVTX)
		printf("%s\"S_ISVTX\"", first ? "" : ", ");
	printf("]");
}

/* ftype_sym is NULL for no file type bits, "" for unknown file type bits. */
static void
test_mknodat(unsigned int mode, const char *ftype_sym)
{
	long rc = syscall(__NR_mknodat, -1, path, mode, 0);
	int err = errno;

	open_event("mknodat", __NR_mknodat);
	jsonl_arg_open(0, "dirfd", side);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "pathname", side);
	printf("\"type\": \"path\", \"value\": ");
	jsonl_bytes(path);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "mode", side);
	printf("\"type\": \"mode\", \"raw\": \"%#03o\"", mode);
	if (!ftype_sym || *ftype_sym) {
		if (ftype_sym)
			printf(", \"filetype\": {\"type\": \"const\","
			       " \"raw\": \"%#o\", \"sym\": \"%s\"}",
			       mode & S_IFMT, ftype_sym);
		print_special(mode);
		printf(", \"perm\": \"%#03o\"",
		       mode & ~(S_IFMT | S_ISUID | S_ISGID | S_ISVTX));
	}
	jsonl_arg_close(side);
	close_event("mknodat", __NR_mknodat);
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");
}

static void
test_umask(unsigned int mask)
{
	unsigned int old = umask(mask);

	open_event("umask", __NR_umask);
	jsonl_arg_open(0, "mask", side);
	printf("\"type\": \"mode\", \"raw\": \"%#03o\"", mask);
	jsonl_arg_close(side);
	close_event("umask", __NR_umask);
	printf("\"return\": {\"type\": \"mode\", \"raw\": \"%#03o\"}}\n", old);
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	test_mknodat(S_IFREG | S_ISUID | 0755, "S_IFREG");
	test_mknodat(S_IFIFO | S_ISGID | S_ISVTX | 0600, "S_IFIFO");
	test_mknodat(0644, NULL);
	test_mknodat(0, NULL);
	test_mknodat(S_IFMT | 0644, "");

	test_umask(022);
	test_umask(0);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_mknodat && __NR_umask")

#endif
