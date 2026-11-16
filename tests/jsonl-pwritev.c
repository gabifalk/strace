/*
 * Check JSONL output of the pwritev syscall.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_pwritev

# include "jsonl.h"

# include <fcntl.h>
# include <stdio.h>
# include <string.h>
# include <sys/uio.h>
# include <unistd.h>

# define LEN 8
# define LIM (LEN - 1)

static bool merged;

static long
k_pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
	return syscall(__NR_pwritev, fd, iov, iovcnt,
		       (kernel_ulong_t) offset,
		       (kernel_ulong_t) ((unsigned long long) offset >> 32));
}

/* The iov_base is printed as a pointer unless readable is set. */
static void
jsonl_iov(const struct iovec *iov, bool readable)
{
	printf("{");
	jsonl_struct_open();

	jsonl_field_open("iov_base", true);
	if (readable)
		jsonl_string_value(iov->iov_base, MIN(iov->iov_len, LIM),
				   iov->iov_len > LIM);
	else
		jsonl_addr(iov->iov_base);
	jsonl_field_close();

	jsonl_field_open("iov_len", false);
	jsonl_unsigned(iov->iov_len);
	jsonl_field_close();

	jsonl_struct_close();
	printf("}");
}

/* Only the first size elements of the array can be read. */
static void
jsonl_iovec(const struct iovec *iov, unsigned int cnt, unsigned int size,
	    bool readable)
{
	unsigned int i;

	if (!iov || (cnt && !size)) {
		jsonl_addr(iov);
		return;
	}

	jsonl_array_open();
	for (i = 0; i < cnt && (i == size || i < LIM); ++i) {
		if (i == size) {
			jsonl_array_fetch_failed(&iov[i]);
			return;
		}
		if (i)
			printf(", ");
		jsonl_iov(&iov[i], readable);
	}
	jsonl_array_close();
	if (i < cnt)
		jsonl_truncated();
}

static void
test_pwritev(const struct iovec *iov, unsigned int cnt, unsigned int size,
	     bool readable, off_t offset)
{
	const char *side = merged ? "entry" : NULL;
	long rc = k_pwritev(0, iov, cnt, offset);
	int saved_errno = errno;

	if (merged)
		jsonl_syscall_merged_open("pwritev", __NR_pwritev);
	else
		jsonl_syscall_open("pwritev", __NR_pwritev, true);

	jsonl_arg_open(0, "fd", side);
	jsonl_fd(0, NULL, false);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(1, "iov", side);
	jsonl_iovec(iov, cnt, size, readable);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(2, "iovcnt", side);
	jsonl_unsigned(cnt);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(3, "offset", side);
	jsonl_decimal(offset);
	jsonl_arg_close(side);

	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("pwritev", __NR_pwritev, false);
	}
	printf("], ");
	errno = saved_errno;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	(void) close(0);
	if (open("/dev/null", O_WRONLY))
		perror_msg_and_fail("open");

	char *buf = tail_alloc(LEN);

	TAIL_ALLOC_OBJECT_VAR_ARR(struct iovec, iov, LEN);
	for (unsigned int i = 0; i < LEN; ++i) {
		buf[i] = i;
		iov[i].iov_base = &buf[i];
		iov[i].iov_len = LEN - i;
	}

	const off_t offset = 0xdefaceddeadbeefLL;

	for (unsigned int i = 0; i < LEN; ++i)
		test_pwritev(iov, i + 1, LEN, true, offset + i);

	/* The array is followed by unreadable memory. */
	for (unsigned int i = 0; i <= LEN; ++i)
		test_pwritev(iov + i, LEN + 1 - i, LEN - i, true,
			     offset + LEN + i);

	iov->iov_base = iov + LEN * 2;
	test_pwritev(iov, 1, 1, false, -1);

	iov += LEN;
	test_pwritev(iov, 42, 0, false, -2);
	test_pwritev(NULL, 1, 0, false, -3);
	test_pwritev(iov, 0, 0, false, -4);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_pwritev")

#endif
