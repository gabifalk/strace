/*
 * Check JSONL output of the bitset returned by EVIOCGBIT ioctl commands:
 * the raw mask, the decoded bits, the truncation of a long list, and the
 * fetched words.
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

# include <limits.h>
# include <stdio.h>
# include <string.h>
# include <linux/input.h>

# define INJECTED_RVAL 8
# define LONG_BITS (sizeof(long) * CHAR_BIT)

static unsigned long *bits;

static void
set_bits(const unsigned int *nrs, size_t count)
{
	memset(bits, 0, INJECTED_RVAL);
	for (size_t i = 0; i < count; ++i)
		bits[nrs[i] / LONG_BITS] |= 1UL << (nrs[i] % LONG_BITS);
}

static void
test_bitset(unsigned int ev, const char *ev_sym, unsigned int max_nr,
	    const unsigned int *nrs, const char *const *syms,
	    const char *sym_pattern, size_t count, bool truncated)
{
	const unsigned int code = EVIOCGBIT(ev, INJECTED_RVAL);
	const unsigned int size_bits =
		INJECTED_RVAL > max_nr / 8 ? max_nr : INJECTED_RVAL * 8;
	const size_t nwords = (size_bits + LONG_BITS - 1) / LONG_BITS;
	const char *side = jsonl_ioctl_merged ? "entry" : NULL;
	unsigned long long raw = 0;

	set_bits(nrs, count);
	for (size_t i = 0; i < count; ++i)
		raw |= 1ULL << nrs[i];

	jsonl_ioctl_rc = ioctl(-1, code, bits);
	jsonl_ioctl_errno = errno;

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
	       "[{\"type\": \"ioctl_cmd\", \"name\": \"EVIOCGBIT\", "
	       "\"args\": [{", code);
	if (ev)
		jsonl_const(ev, true, ev_sym, NULL);
	else
		jsonl_unsigned(0);
	printf("}, {");
	jsonl_unsigned(INJECTED_RVAL);
	printf("}]}]");
	jsonl_ioctl_dir(code);
	printf(", \"ioc_type\": \"%#x\", \"ioc_nr\": \"%#x\","
	       " \"ioc_size\": \"%#x\"",
	       _IOC_TYPE(code), _IOC_NR(code), _IOC_SIZE(code));
	jsonl_arg_close(side);

	jsonl_ioctl_argp_open("argp", true);
	printf("\"type\": \"bitset\", \"raw\": \"%#llx\", \"elems\": [",
	       raw);
	for (size_t i = 0; i < count - truncated; ++i) {
		printf("%s{", i ? ", " : "");
		jsonl_const(nrs[i], true, syms[i],
			    syms[i] ? NULL : sym_pattern);
		printf("}");
	}
	printf("]");
	if (truncated)
		jsonl_truncated();
	printf(", \"words\": {");
	jsonl_array_open();
	for (size_t i = 0; i < nwords; ++i) {
		printf("%s{\"type\": \"hex\", \"raw\": \"%#lx\"}",
		       i ? ", " : "", bits[i]);
	}
	jsonl_array_close();
	printf("}");
	jsonl_ioctl_argp_close();

	printf("], ");
	jsonl_return_injected("unsigned", jsonl_ioctl_rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	bits = tail_alloc(INJECTED_RVAL);

	static const unsigned int ev_nrs[] = { EV_KEY, EV_ABS, 0x6 };
	static const char *const ev_syms[] = { "EV_KEY", "EV_ABS", NULL };
	test_bitset(0, NULL, EV_MAX, ev_nrs, ev_syms, "EV_???",
		    ARRAY_SIZE(ev_nrs), false);

	static const unsigned int key_nrs[] = {
		KEY_ESC, KEY_1, KEY_2, KEY_3, KEY_4
	};
	static const char *const key_syms[] = {
		"KEY_ESC", "KEY_1", "KEY_2", "KEY_3", "KEY_4"
	};
	test_bitset(EV_KEY, "EV_KEY", KEY_MAX, key_nrs, key_syms, "KEY_???",
		    ARRAY_SIZE(key_nrs), true);

	test_bitset(EV_REL, "EV_REL", REL_MAX, NULL, NULL, "REL_???", 0,
		    false);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
