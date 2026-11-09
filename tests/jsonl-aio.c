/*
 * Check JSONL output of an io_submit array that fails to be fetched.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_io_submit

# include "jsonl.h"

# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <linux/aio_abi.h>

static bool merged;
static const char *side;

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	static struct iocb cb;
	memset(&cb, 0, sizeof(cb));
	cb.aio_lio_opcode = IOCB_CMD_NOOP;
	cb.aio_fildes = (uint32_t) -1;

	/*
	 * A one-element array of iocb pointers placed at the end of a mapped
	 * page: the first element is fetched, the second one is not.
	 */
	unsigned long *const iocbpp = tail_alloc(sizeof(*iocbpp));
	iocbpp[0] = (unsigned long) (uintptr_t) &cb;

	const long rc = syscall(__NR_io_submit, (unsigned long) 0, (long) 2,
				iocbpp);
	const int err = errno;

	if (merged)
		jsonl_syscall_merged_open("io_submit", __NR_io_submit);
	else
		jsonl_syscall_open("io_submit", __NR_io_submit, true);

	jsonl_arg_open(0, "ctx_id", side);
	printf("\"type\": \"addr\", \"raw\": null");
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(1, "nr", side);
	printf("\"type\": \"decimal\", \"raw\": \"2\"");
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(2, "iocbpp", side);
	printf("\"type\": \"array\", \"elems\": [");
	printf("{\"type\": \"struct\", \"fields\": {");
	printf("\"aio_data\": {\"type\": \"hex\", \"raw\": \"0\"}, ");
	printf("\"aio_lio_opcode\": {\"type\": \"const\", \"raw\": \"6\""
	       ", \"sym\": \"IOCB_CMD_NOOP\"}, ");
	printf("\"aio_fildes\": {\"type\": \"fd\", \"raw\": \"-1\"}");
	printf("}}, {}], ");
	printf("\"fetch_failed_addr\": \"%#llx\"",
	       (unsigned long long) (uintptr_t) (iocbpp + 1));
	jsonl_arg_close(side);

	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("io_submit", __NR_io_submit, false);
	}
	printf("], ");
	errno = err;
	jsonl_return("decimal", rc);
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_io_submit")

#endif
