/*
 * Check JSONL output of the adjtimex syscall.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"
#include "scno.h"

#ifdef __NR_adjtimex

# include <stddef.h>
# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/timex.h>

# include "kernel_old_timex.h"
# include "xlat.h"
# include "xlat/adjtimex_state.h"
# include "xlat/adjtimex_status.h"

static bool merged;

static const char *
xlat_lookup(const struct xlat *xlat, uint64_t val)
{
	for (size_t i = 0; i < xlat->size; ++i)
		if (xlat->data[i].str && xlat->data[i].val == val)
			return xlat->data[i].str;
	return NULL;
}

static void
emit_decimal(intmax_t val)
{
	printf("{\"type\": \"decimal\", \"raw\": \"%jd\"}", val);
}

static void
emit_status(uint64_t flags)
{
	uint64_t left = flags;
	int n = 0;

	jsonl_flags_open(flags);
	for (size_t i = 0; (left || !n) && i < adjtimex_status->size; ++i) {
		const uint64_t v = adjtimex_status->data[i].val;

		if (adjtimex_status->data[i].str
		    && ((left == v) || (v && (left & v) == v))) {
			jsonl_flags_const(v, adjtimex_status->data[i].str,
					  n == 0);
			++n;
			left &= ~v;
		}
		if (!left)
			break;
	}
	if (left)
		jsonl_flags_remainder(left, NULL, n == 0);
	jsonl_flags_close();
}

static void
emit_timex(const kernel_old_timex_t *tx)
{
	printf("\"type\": \"struct\", \"fields\": {");
	printf("\"modes\": {");
	jsonl_flags_open((unsigned int) tx->modes);
	jsonl_flags_close();
	printf("}");
	printf(", \"offset\": ");
	emit_decimal((intmax_t) tx->offset);
	printf(", \"freq\": ");
	emit_decimal((intmax_t) tx->freq);
	printf(", \"maxerror\": ");
	emit_decimal((intmax_t) tx->maxerror);
	printf(", \"esterror\": ");
	emit_decimal((intmax_t) tx->esterror);
	printf(", \"status\": {");
	emit_status((unsigned int) tx->status);
	printf("}");
	printf(", \"constant\": ");
	emit_decimal((intmax_t) tx->constant);
	printf(", \"precision\": ");
	emit_decimal((intmax_t) tx->precision);
	printf(", \"tolerance\": ");
	emit_decimal((intmax_t) tx->tolerance);
	printf(", \"time\": {\"type\": \"struct\", \"fields\": {\"tv_sec\": ");
	printf("{\"type\": \"decimal\", \"raw\": \"%lld\"}",
	       (long long) tx->time.tv_sec);
	printf(", \"tv_usec\": {\"type\": \"unsigned\", \"raw\": \"%llu\"}}}",
	       zero_extend_signed_to_ull(tx->time.tv_usec));
	printf(", \"tick\": ");
	emit_decimal((intmax_t) tx->tick);
	printf(", \"ppsfreq\": ");
	emit_decimal((intmax_t) tx->ppsfreq);
	printf(", \"jitter\": ");
	emit_decimal((intmax_t) tx->jitter);
	printf(", \"shift\": ");
	emit_decimal((intmax_t) tx->shift);
	printf(", \"stabil\": ");
	emit_decimal((intmax_t) tx->stabil);
	printf(", \"jitcnt\": ");
	emit_decimal((intmax_t) tx->jitcnt);
	printf(", \"calcnt\": ");
	emit_decimal((intmax_t) tx->calcnt);
	printf(", \"errcnt\": ");
	emit_decimal((intmax_t) tx->errcnt);
	printf(", \"stbcnt\": ");
	emit_decimal((intmax_t) tx->stbcnt);
	printf(", \"tai\": ");
	emit_decimal((intmax_t) tx->tai);
	printf("}");
}

static void
emit_call(const kernel_old_timex_t *tx, long state)
{
	const char *side = merged ? "exit" : NULL;

	if (merged) {
		jsonl_syscall_merged_open("adjtimex", __NR_adjtimex);
	} else {
		jsonl_syscall_open("adjtimex", __NR_adjtimex, true);
		printf("]}\n");
		jsonl_syscall_open("adjtimex", __NR_adjtimex, false);
	}
	jsonl_arg_open(0, "buf", side);
	if (tx)
		emit_timex(tx);
	else
		printf("\"type\": \"addr\", \"raw\": null");
	jsonl_arg_close(side);
	printf("], ");
	if (!tx) {
		jsonl_return("decimal", state);
	} else {
		printf("\"return\": {");
		jsonl_const((uint64_t) state, false,
			    xlat_lookup(adjtimex_state, (uint64_t) state), NULL);
		printf("}");
	}
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	emit_call(NULL, syscall(__NR_adjtimex, (unsigned long) NULL));

	TAIL_ALLOC_OBJECT_CONST_PTR(kernel_old_timex_t, tx);
	memset(tx, 0, sizeof(*tx));

	long state = syscall(__NR_adjtimex, (unsigned long) (uintptr_t) tx);
	if (state < 0)
		perror_msg_and_skip("adjtimex");
	emit_call(tx, state);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_adjtimex")

#endif
