/*
 * Check JSONL output of the classic BPF program of seccomp
 * SECCOMP_SET_MODE_FILTER: statement and jump instructions, the parts
 * of instruction codes, and seccomp return actions with and without data.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_seccomp

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <linux/filter.h>
# include <linux/seccomp.h>

static void
print_group(unsigned int raw, const char *sym, const char *sym_pattern,
	    bool first)
{
	printf("%s{\"raw\": ", first ? "" : ", ");
	jsonl_xlat_raw(raw);
	printf(", \"group_type\": \"flags\", \"elems\": [{");
	jsonl_const(raw, true, sym, sym_pattern);
	printf("}]}");
}

static void
print_insn_open(bool jump, unsigned int code)
{
	printf("{\"type\": \"bpf_insn\", \"kind\": \"%s\", \"code\": {"
	       "\"type\": \"flags\", \"raw\": ", jump ? "jump" : "stmt");
	jsonl_xlat_raw(code);
	printf(", \"groups\": [");
}

static void
print_insn_k_open(void)
{
	printf("]}, \"k\": {");
}

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");
	const char *const side = merged ? "entry" : NULL;

	static const struct sock_filter insns[] = {
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS, 4),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | 1),
		BPF_STMT(BPF_ST | 0x8, 0),
		BPF_JUMP(BPF_JMP | BPF_K | BPF_JEQ, 0x3c, 1, 2),
	};
	TAIL_ALLOC_OBJECT_CONST_PTR(struct sock_fprog, prog);
	struct sock_filter *const filter =
		tail_memdup(insns, sizeof(insns));
	prog->len = ARRAY_SIZE(insns);
	prog->filter = filter;

	long rc = syscall(__NR_seccomp, SECCOMP_SET_MODE_FILTER, 0, prog);

	if (merged)
		jsonl_syscall_merged_open("seccomp", __NR_seccomp);
	else
		jsonl_syscall_open("seccomp", __NR_seccomp, true);
	jsonl_arg_open(0, "operation", side);
	jsonl_const(SECCOMP_SET_MODE_FILTER, true, "SECCOMP_SET_MODE_FILTER",
		    NULL);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "flags", side);
	jsonl_flags_open(0);
	jsonl_flags_close();
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "args", side);
	jsonl_struct_open();
	jsonl_field_unsigned("len", true, prog->len);
	printf(", \"filter\": {");
	jsonl_array_open();

	print_insn_open(false, BPF_LD | BPF_W | BPF_ABS);
	print_group(BPF_LD, "BPF_LD", NULL, true);
	print_group(BPF_W, "BPF_W", NULL, false);
	print_group(BPF_ABS, "BPF_ABS", NULL, false);
	print_insn_k_open();
	jsonl_hex(4);
	printf("}}, ");

	print_insn_open(false, BPF_RET | BPF_K);
	print_group(BPF_RET, "BPF_RET", NULL, true);
	print_group(BPF_K, "BPF_K", NULL, false);
	print_insn_k_open();
	jsonl_const(SECCOMP_RET_ALLOW, true, "SECCOMP_RET_ALLOW", NULL);
	printf("}}, ");

	print_insn_open(false, BPF_RET | BPF_K);
	print_group(BPF_RET, "BPF_RET", NULL, true);
	print_group(BPF_K, "BPF_K", NULL, false);
	print_insn_k_open();
	jsonl_flags_groups_open(SECCOMP_RET_ERRNO | 1);
	print_group(SECCOMP_RET_ERRNO, "SECCOMP_RET_ERRNO", NULL, true);
	printf(", {\"raw\": \"0x1\", \"group_type\": \"flags\","
	       " \"elems\": [{");
	jsonl_hex(1);
	printf("}]}");
	jsonl_flags_groups_close();
	printf("}}, ");

	print_insn_open(false, BPF_ST | 0x8);
	print_group(BPF_ST, "BPF_ST", NULL, true);
	print_group(0x8, NULL, "BPF_???", false);
	print_insn_k_open();
	jsonl_hex(0);
	printf("}}, ");

	print_insn_open(true, BPF_JMP | BPF_K | BPF_JEQ);
	print_group(BPF_JMP, "BPF_JMP", NULL, true);
	print_group(BPF_K, "BPF_K", NULL, false);
	print_group(BPF_JEQ, "BPF_JEQ", NULL, false);
	print_insn_k_open();
	jsonl_hex(0x3c);
	printf("}, \"jt\": \"1\", \"jf\": \"2\"}");

	jsonl_array_close();
	printf("}");
	jsonl_struct_close();
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("seccomp", __NR_seccomp, false);
	}
	printf("], ");
	jsonl_return("decimal", rc);
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_seccomp")

#endif
