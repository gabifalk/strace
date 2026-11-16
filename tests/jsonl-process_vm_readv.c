/*
 * Check JSONL output of the process_vm_readv syscall.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

/* jsonl-process_vm_writev.c includes this file with its own definitions. */
#ifndef OP_STR
# define OP_STR "process_vm_readv"
# define OP_WR 0
# ifdef __NR_process_vm_readv
#  define OP_NR __NR_process_vm_readv
# endif
#endif

#ifdef OP_NR

# include "jsonl.h"

# include <inttypes.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/uio.h>

# if OP_WR
#  define in_iovec  rmt_iovec
#  define out_iovec lcl_iovec
#  define in_iov    rmt_iov
#  define out_iov   lcl_iov
# else
#  define in_iovec  lcl_iovec
#  define out_iovec rmt_iovec
#  define in_iov    lcl_iov
#  define out_iov   rmt_iov
# endif

enum { MAX_SEGM_COUNT = 2, MAX_STR_LEN = 5 };

struct print_iov_arg {
	uint32_t count;
	uint32_t valid    :1,
		 string   :1,
		 addr_term:1,
		 check_rc :1;
	uint32_t str_segms;
	uint8_t  str_base[MAX_SEGM_COUNT];
	uint8_t  str_size[MAX_SEGM_COUNT];
};

static bool merged;

static void
jsonl_iov(const struct iovec *iov, const struct print_iov_arg *arg, long rc)
{
	uint32_t num_segm = 0;
	uint64_t segm_offs = 0;

	if (!arg || !arg->valid) {
		jsonl_addr(iov);
		return;
	}

	jsonl_array_open();

	for (uint32_t i = 0; i < arg->count; ++i) {
		if (i >= MAX_STR_LEN) {
			jsonl_array_close();
			jsonl_truncated();
			return;
		}

		printf("%s{", i ? ", " : "");
		jsonl_struct_open();
		jsonl_field_open("iov_base", true);
		if (arg->string && (!arg->check_rc || (rc != -1))) {
			unsigned char str[MAX_STR_LEN];
			size_t len = 0;

			for (uint64_t left = iov[i].iov_len; left; --left) {
				if ((num_segm >= arg->str_segms) ||
				    (num_segm >= MAX_SEGM_COUNT))
					error_msg_and_fail("jsonl_iov: segment "
							   "count overrun");

				if (len < MAX_STR_LEN)
					str[len++] = arg->str_base[num_segm]
						     + segm_offs;

				if (++segm_offs >= arg->str_size[num_segm]) {
					num_segm++;
					segm_offs = 0;
				}
			}

			jsonl_string_value(str, len,
					   iov[i].iov_len > MAX_STR_LEN);
		} else {
			jsonl_addr(iov[i].iov_base);
		}
		jsonl_field_close();
		jsonl_field_open("iov_len", false);
		jsonl_unsigned(iov[i].iov_len);
		jsonl_field_close();
		jsonl_struct_close();
		printf("}");
	}

	if (arg->addr_term) {
		jsonl_array_fetch_failed(iov + arg->count);
		return;
	}

	jsonl_array_close();
}

/* All arguments but pid are printed on exiting unless OP_WR is set. */
static void
do_call(kernel_ulong_t pid,
	kernel_ulong_t local_iov, const struct print_iov_arg *local_arg,
	kernel_ulong_t liovcnt,
	kernel_ulong_t remote_iov, const struct print_iov_arg *remote_arg,
	kernel_ulong_t riovcnt,
	kernel_ulong_t flags)
{
	const char *side = merged ? "entry" : NULL;

	const long rc = syscall(OP_NR, pid, local_iov, liovcnt, remote_iov,
				riovcnt, flags);
	const int saved_errno = errno;

	if (merged)
		jsonl_syscall_merged_open(OP_STR, OP_NR);
	else
		jsonl_syscall_open(OP_STR, OP_NR, true);

	jsonl_arg_open(0, "pid", side);
	jsonl_pid("tgid", (int) pid);
	jsonl_arg_close(side);

	if (OP_WR) {
		printf(", ");
	} else if (merged) {
		printf(", ");
		side = "exit";
	} else {
		printf("]}\n");
		jsonl_syscall_open(OP_STR, OP_NR, false);
	}

	jsonl_arg_open(1, "local_iov", side);
	jsonl_iov((const struct iovec *) (uintptr_t) local_iov, local_arg, rc);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(2, "liovcnt", side);
	jsonl_unsigned((unsigned long) liovcnt);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(3, "remote_iov", side);
	jsonl_iov((const struct iovec *) (uintptr_t) remote_iov, remote_arg,
		  rc);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(4, "riovcnt", side);
	jsonl_unsigned((unsigned long) riovcnt);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(5, "flags", side);
	jsonl_unsigned((unsigned long) flags);
	jsonl_arg_close(side);

	if (OP_WR && !merged) {
		printf("]}\n");
		jsonl_syscall_open(OP_STR, OP_NR, false);
	}

	printf("], ");
	errno = saved_errno;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

static kernel_ulong_t
ptr_cast(void *ptr)
{
	return (kernel_ulong_t) (uintptr_t) ptr;
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	enum {
		SIZE_11 = 2,
		SIZE_12 = 3,
		SIZE_13 = 4,
		SIZE_1 = SIZE_11 + SIZE_12 + SIZE_13,
		SIZE_21 = 5,
		SIZE_22 = 6,
		SIZE_23 = 7,
		SIZE_2 = SIZE_21 + SIZE_22 + SIZE_23,
	};

	enum {
		SEGM1_BASE = 0x80,
		SEGM2_BASE = 0xA0,
	};

	static const kernel_ulong_t bogus_pid =
		(kernel_ulong_t) 0xbadfaceddeadca57ULL;
	static const kernel_ulong_t bogus_iovcnt1 =
		(kernel_ulong_t) 0xdec0ded1defaced2ULL;
	static const kernel_ulong_t bogus_iovcnt2 =
		(kernel_ulong_t) 0xdec0ded3defaced4ULL;
	static const kernel_ulong_t bogus_flags =
		(kernel_ulong_t) 0xdeadc0deda7adeadULL;

	pid_t my_pid = getpid();
	char *data1_out = tail_alloc(SIZE_1);
	char *data2_out = tail_alloc(SIZE_2);
	char *data1_in  = tail_alloc(SIZE_2);
	char *data2_in  = tail_alloc(SIZE_1);

	struct iovec bogus_iovec[] = {
		{ data1_out + SIZE_1, (size_t) 0xdeadfaceca57beefULL },
		{ data1_in  + SIZE_2, (size_t) 0xbadc0dedda7adeadULL },
		{ data2_out + SIZE_2, (size_t) 0xf157facedec0ded1ULL },
		{ data2_in  + SIZE_1, (size_t) 0xdefaced2bea7be57ULL },
	};

	struct iovec out_iovec[] = {
		{ data1_out,  SIZE_11 },
		{ data1_out + SIZE_11,  SIZE_12 },
		{ data1_out + SIZE_11 + SIZE_12,  SIZE_13 },
		{ data2_out,  SIZE_21 },
		{ data2_out + SIZE_21,  SIZE_22 },
		{ data2_out + SIZE_21 + SIZE_22,  SIZE_23 },
	};
	struct iovec in_iovec[] = {
		{ data1_in,  SIZE_23 },
		{ data1_in + SIZE_23,  SIZE_22 },
		{ data1_in + SIZE_23 + SIZE_22,  SIZE_21 },
		{ data2_in,  SIZE_13 },
		{ data2_in + SIZE_13,  SIZE_12 },
		{ data2_in + SIZE_13 + SIZE_12,  SIZE_11 },
	};

	struct iovec *bogus_iov = tail_memdup(bogus_iovec, sizeof(bogus_iovec));
	struct iovec *lcl_iov   = tail_memdup(lcl_iovec,   sizeof(lcl_iovec));
	struct iovec *rmt_iov   = tail_memdup(rmt_iovec,   sizeof(rmt_iovec));

	const struct print_iov_arg empty_arg = { 0, 1 };
	struct print_iov_arg bogus_arg   = { ARRAY_SIZE(bogus_iovec), 1 };
	struct print_iov_arg lcl_arg     = { ARRAY_SIZE(lcl_iovec), 1, 1, 0, 0,
		2, {SEGM1_BASE, SEGM2_BASE}, {SIZE_1, SIZE_2} };
	struct print_iov_arg rmt_arg     = { ARRAY_SIZE(rmt_iovec), 1 };

	struct print_iov_arg bogus_arg_cut = {
		ARRAY_SIZE(bogus_iovec) - 2, 1, 0, 1
	};
	struct print_iov_arg lcl_arg_cut = {
		ARRAY_SIZE(lcl_iovec) - 2, 1, 1, 1, 0, 2,
		{ SEGM1_BASE + SIZE_11 + SIZE_12, SEGM2_BASE },
		{SIZE_13, SIZE_2}
	};
	struct print_iov_arg rmt_arg_cut = { ARRAY_SIZE(rmt_iovec) - 2, 1 };

	fill_memory_ex(data1_out, SIZE_1, SEGM1_BASE, SIZE_1);
	fill_memory_ex(data2_out, SIZE_2, SEGM2_BASE, SIZE_2);

	do_call(bogus_pid, 0, NULL, bogus_iovcnt1, 0, NULL, bogus_iovcnt2,
		bogus_flags);

	do_call(my_pid, ptr_cast(bogus_iov + ARRAY_SIZE(bogus_iovec)),
		&empty_arg, 0, ptr_cast(in_iov + ARRAY_SIZE(in_iovec)),
		&empty_arg, 0, 0);
	do_call(my_pid, ptr_cast(bogus_iov + ARRAY_SIZE(bogus_iovec)),
		NULL, bogus_iovcnt1, ptr_cast(in_iov + ARRAY_SIZE(in_iovec)),
		NULL, bogus_iovcnt2, 0);

	do_call(my_pid, ptr_cast(bogus_iov), &bogus_arg,
		ARRAY_SIZE(bogus_iovec), ptr_cast(rmt_iov + 2),
		&rmt_arg_cut, ARRAY_SIZE(rmt_iovec) - 2, 0);

# if !OP_WR
	lcl_arg.check_rc = 1;
	lcl_arg_cut.check_rc = 1;
# endif

	do_call(my_pid, ptr_cast(lcl_iov + 2), &lcl_arg_cut,
		ARRAY_SIZE(lcl_iovec) - 1, ptr_cast(bogus_iov + 2),
		&bogus_arg_cut, ARRAY_SIZE(bogus_iovec) - 1, 0);

	lcl_arg_cut.addr_term = 0;

	rmt_arg_cut.addr_term = 1;
	rmt_arg_cut.count = 5;

	do_call(my_pid, ptr_cast(lcl_iov + 2), &lcl_arg_cut,
		ARRAY_SIZE(lcl_iovec) - 2, ptr_cast(rmt_iov + 1),
		&rmt_arg_cut, ARRAY_SIZE(rmt_iovec), 0);

	/* Correct call */
	do_call(my_pid, ptr_cast(lcl_iov), &lcl_arg,
		ARRAY_SIZE(lcl_iovec), ptr_cast(rmt_iov), &rmt_arg,
		ARRAY_SIZE(rmt_iovec), 0);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED(OP_STR)

#endif
