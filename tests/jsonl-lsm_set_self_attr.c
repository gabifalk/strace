/*
 * Check JSONL output of the lsm_set_self_attr syscall.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"
#include "scno.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/lsm.h>

static const kernel_ulong_t bad = (kernel_ulong_t) 0xbadc0dedbadc0dedULL;

static bool merged;

enum ctx_kind {
	CTX_ADDR,	/* the argument is printed as a pointer */
	CTX_NO_DATA,	/* a structure without the ctx field */
	CTX_DATA_ADDR,	/* the ctx field is printed as a pointer */
	CTX_DATA	/* the ctx field is printed as bytes */
};

static void
jsonl_lsm_ctx(const struct lsm_ctx *ctx, enum ctx_kind kind)
{
	jsonl_struct_open();

	jsonl_field_const("id", true, ctx->id, NULL, NULL);

	jsonl_field_hex("flags", false, ctx->flags);

	jsonl_field_unsigned("len", false, ctx->len);

	jsonl_field_unsigned("ctx_len", false, ctx->ctx_len);

	if (kind != CTX_NO_DATA) {
		jsonl_field_open("ctx", false);
		if (kind == CTX_DATA)
			jsonl_bytes_value(ctx->ctx, ctx->ctx_len, false);
		else
			jsonl_addr(ctx->ctx);
		jsonl_field_close();
	}

	jsonl_struct_close();
}

static void
test_lsm_set_self_attr(const unsigned int attr, const char *attr_str,
		       const struct lsm_ctx *ctx, enum ctx_kind kind,
		       const uint32_t size, const uint32_t flags)
{
	const kernel_ulong_t fill = (kernel_ulong_t) 0xdefaced00000000ULL;
	const kernel_ulong_t arg1 = fill | attr;
	const kernel_ulong_t arg2 = (uintptr_t) ctx;
	const kernel_ulong_t arg3 = fill | size;
	const kernel_ulong_t arg4 = fill | flags;
	const char *side = merged ? "entry" : NULL;
	const long rc = syscall(__NR_lsm_set_self_attr,
				arg1, arg2, arg3, arg4, bad, bad);
	int saved_errno = errno;

	if (merged)
		jsonl_syscall_merged_open("lsm_set_self_attr",
					  __NR_lsm_set_self_attr);
	else
		jsonl_syscall_open("lsm_set_self_attr",
				   __NR_lsm_set_self_attr, true);

	jsonl_arg_open(0, "attr", side);
	jsonl_const(attr, true, attr_str, "LSM_ATTR_???");
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(1, "ctx", side);
	if (kind == CTX_ADDR)
		jsonl_addr(ctx);
	else
		jsonl_lsm_ctx(ctx, kind);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(2, "size", side);
	jsonl_unsigned(size);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(3, "flags", side);
	jsonl_hex(flags);
	jsonl_arg_close(side);

	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("lsm_set_self_attr",
				   __NR_lsm_set_self_attr, false);
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

	test_lsm_set_self_attr(ARG_STR(LSM_ATTR_UNDEF), NULL, CTX_ADDR, 0, 0);

	struct lsm_ctx *const ctx =
		midtail_alloc(sizeof(*ctx), 2);
	fill_memory(ctx, sizeof(*ctx));
	const void *const bad_ctx = (void *) ctx + 1;

	const unsigned int bad_attr = 0xfacefed1;
	const uint32_t bad_flags = 0xfacefed2;
	uint32_t size = 0xdeadbeef;

	test_lsm_set_self_attr(bad_attr, NULL, bad_ctx, CTX_ADDR,
			       size, bad_flags);

	ctx->ctx_len = 0;
	test_lsm_set_self_attr(ARG_STR(LSM_ATTR_CURRENT), ctx, CTX_NO_DATA,
			       size, 1);

	/* The ctx data is not readable. */
	ctx->ctx_len = 1;
	test_lsm_set_self_attr(ARG_STR(LSM_ATTR_CURRENT), ctx, CTX_DATA_ADDR,
			       size, 2);

	ctx->ctx_len = 2;
	struct lsm_ctx *ctx2 = (void *) ctx - ctx->ctx_len;
	memmove(ctx2, ctx, sizeof(*ctx));
	fill_memory((void *) (ctx + 1) - 2, 2);
	test_lsm_set_self_attr(ARG_STR(LSM_ATTR_CURRENT), ctx2, CTX_DATA,
			       size, 3);

	jsonl_exit_event(0);
	return 0;
}
