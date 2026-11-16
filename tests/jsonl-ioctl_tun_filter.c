/*
 * Check JSONL output of the classic BPF program of the TUNATTACHFILTER
 * ioctl command: statement and jump instructions of a socket filter.
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

# include <stdio.h>
# include <string.h>
# include <linux/filter.h>
# include <linux/if_tun.h>

static void
print_group(unsigned int raw, const char *sym, bool first)
{
	printf("%s{\"raw\": ", first ? "" : ", ");
	jsonl_xlat_raw(raw);
	printf(", \"group_type\": \"flags\", \"elems\": [{");
	jsonl_const(raw, true, sym, NULL);
	printf("}]}");
}

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	static const struct sock_filter insns[] = {
		BPF_JUMP(BPF_JMP | BPF_K | BPF_JGE, 0x64, 0, 3),
		BPF_STMT(BPF_RET | BPF_K, 0xffff),
	};
	TAIL_ALLOC_OBJECT_CONST_PTR(struct sock_fprog, prog);
	prog->len = ARRAY_SIZE(insns);
	prog->filter = tail_memdup(insns, sizeof(insns));

	jsonl_ioctl_call(TUNATTACHFILTER, "TUNATTACHFILTER", NULL, prog);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_struct_open();
	jsonl_field_unsigned("len", true, prog->len);
	printf(", \"filter\": {");
	jsonl_array_open();

	printf("{\"type\": \"bpf_insn\", \"kind\": \"jump\", \"code\": {");
	jsonl_flags_groups_open(BPF_JMP | BPF_K | BPF_JGE);
	print_group(BPF_JMP, "BPF_JMP", true);
	print_group(BPF_K, "BPF_K", false);
	print_group(BPF_JGE, "BPF_JGE", false);
	jsonl_flags_groups_close();
	printf("}, \"k\": {");
	jsonl_hex(0x64);
	printf("}, \"jt\": \"0\", \"jf\": \"3\"}, ");

	printf("{\"type\": \"bpf_insn\", \"kind\": \"stmt\", \"code\": {");
	jsonl_flags_groups_open(BPF_RET | BPF_K);
	print_group(BPF_RET, "BPF_RET", true);
	print_group(BPF_K, "BPF_K", false);
	jsonl_flags_groups_close();
	printf("}, \"k\": {");
	jsonl_hex(0xffff);
	printf("}}");

	jsonl_array_close();
	printf("}");
	jsonl_struct_close();
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
