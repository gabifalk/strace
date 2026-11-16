/*
 * Check JSONL output of the move_pages syscall.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_move_pages

# include "jsonl.h"

# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

# define MAX_STRLEN 3

static bool merged;

static void
jsonl_page_array(const void **const pages,
		 const unsigned long count,
		 const unsigned int offset)
{
	if (!pages || (count && count <= offset)) {
		jsonl_addr(pages);
		return;
	}
	jsonl_array_open();
	for (unsigned long i = 0; i < count; ++i) {
		if (i + offset < count && i >= MAX_STRLEN) {
			jsonl_array_close();
			jsonl_truncated();
			return;
		}
		if (i + offset >= count) {
			jsonl_array_fetch_failed(pages + i);
			return;
		}
		if (i)
			printf(", ");
		printf("{");
		jsonl_addr(pages[i]);
		printf("}");
	}
	jsonl_array_close();
}

static void
jsonl_node_array(const int *const nodes,
		 const unsigned long count,
		 const unsigned int offset)
{
	if (!nodes || (count && count <= offset)) {
		jsonl_addr(nodes);
		return;
	}
	jsonl_array_open();
	for (unsigned long i = 0; i < count; ++i) {
		if (i + offset < count && i >= MAX_STRLEN) {
			jsonl_array_close();
			jsonl_truncated();
			return;
		}
		if (i + offset >= count) {
			jsonl_array_fetch_failed(nodes + i);
			return;
		}
		if (i)
			printf(", ");
		printf("{");
		jsonl_decimal(nodes[i]);
		printf("}");
	}
	jsonl_array_close();
}

static void
jsonl_status_array(const int *const status, const unsigned long count)
{
	jsonl_array_open();
	for (unsigned long i = 0; i < count; ++i) {
		if (i >= MAX_STRLEN) {
			jsonl_array_close();
			jsonl_truncated();
			return;
		}
		printf("%s{", i ? ", " : "");
		if (status[i] >= 0)
			jsonl_decimal(status[i]);
		else
			jsonl_errno(status[i]);
		printf("}");
	}
	jsonl_array_close();
}

/* status is printed as an array when status_array is set. */
static void
jsonl_move_pages(const unsigned long pid, const unsigned long count,
		 const unsigned int offset, const void **const pages,
		 const int *const nodes, const int *const status,
		 const bool status_array, const unsigned int flags,
		 const char *const flags_str, const long rc)
{
	const char *side = merged ? "entry" : NULL;
	const int saved_errno = errno;

	if (merged)
		jsonl_syscall_merged_open("move_pages", __NR_move_pages);
	else
		jsonl_syscall_open("move_pages", __NR_move_pages, true);

	jsonl_arg_open(0, "pid", side);
	jsonl_pid("tgid", (int) pid);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(1, "count", side);
	jsonl_unsigned(count);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(2, "pages", side);
	jsonl_page_array(pages, count, offset);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(3, "nodes", side);
	jsonl_node_array(nodes, count, offset);
	jsonl_arg_close(side);

	if (merged) {
		printf(", ");
		side = "exit";
	} else {
		printf("]}\n");
		jsonl_syscall_open("move_pages", __NR_move_pages, false);
	}

	jsonl_arg_open(4, "status", side);
	if (status_array)
		jsonl_status_array(status, count);
	else
		jsonl_addr(status);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(5, "flags", side);
	jsonl_flags_open(flags);
	jsonl_flags_const(flags, flags_str, true);
	jsonl_flags_close();
	jsonl_arg_close(side);

	printf("], ");
	errno = saved_errno;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

static void
test_stat_pages(const unsigned long pid, const unsigned long count,
		const void **const pages, int *const status)
{
	const unsigned long flags = (unsigned long) 0xfacefeed00000002ULL;

	long rc = syscall(__NR_move_pages,
			  pid, count, pages, NULL, status, flags);

	jsonl_move_pages(pid, count, 0, pages, NULL, status, !count || !rc,
			 (unsigned int) flags, "MPOL_MF_MOVE", rc);
}

static void
test_move_pages(const unsigned long pid, unsigned long count,
		const unsigned int offset, const void **const pages,
		int *const nodes, int *const status)
{
	const unsigned long flags = (unsigned long) 0xfacefeed00000004ULL;
	count += offset;

	long rc = syscall(__NR_move_pages,
			  pid, count, pages, nodes, status, flags);

	jsonl_move_pages(pid, count, offset, pages, nodes, status, !count,
			 (unsigned int) flags, "MPOL_MF_MOVE_ALL", rc);
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	const unsigned long pid =
		(unsigned long) 0xfacefeed00000000ULL | getpid();
	unsigned long count = 1;
	const unsigned page_size = get_page_size();
	const void *const page = tail_alloc(page_size);
	const void *const efault = page + page_size;
	TAIL_ALLOC_OBJECT_VAR_PTR(const void *, pages);
	TAIL_ALLOC_OBJECT_VAR_PTR(int, nodes);
	TAIL_ALLOC_OBJECT_VAR_PTR(int, status);

	test_stat_pages(pid, 0, pages, status);
	test_move_pages(pid, 0, 0, pages, nodes, status);
	test_move_pages(pid, 0, 1, pages + 1, nodes + 1, status + 1);

	*pages = page;
	test_stat_pages(pid, count, pages, status);
	*nodes = 0xdeadbee1;
	test_move_pages(pid, count, 0, pages, nodes, status);
	test_move_pages(pid, count, 1, pages, nodes, status);

	++count;
	--status;
	*(--pages) = efault;
	test_stat_pages(pid, count, pages, status);
	*(--nodes) = 0xdeadbee2;
	test_move_pages(pid, count, 0, pages, nodes, status);
	test_move_pages(pid, count, 1, pages, nodes, status);

	++count;
	--status;
	*(--pages) = nodes;
	test_stat_pages(pid, count, pages, status);
	*(--nodes) = 0xdeadbee3;
	test_move_pages(pid, count, 0, pages, nodes, status);
	test_move_pages(pid, count, 1, pages, nodes, status);

	++count;
	--status;
	*(--pages) = status;
	test_stat_pages(pid, count, pages, status);
	*(--nodes) = 0xdeadbee4;
	test_move_pages(pid, count, 0, pages, nodes, status);
	test_move_pages(pid, count, 1, pages, nodes, status);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_move_pages")

#endif
