/*
 * Check JSONL output of socket filter options: the special load offsets
 * of a socket filter attached with setsockopt SO_ATTACH_FILTER, and the
 * optlen of getsockopt SO_GET_FILTER changed by the kernel.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_setsockopt && defined __NR_getsockopt

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/socket.h>
# include <linux/filter.h>

static bool merged;

static void
open_sockopt(const char *name, long scno, int fd, unsigned int optname,
	     const char *optname_sym)
{
	const char *const side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open(name, scno);
	else
		jsonl_syscall_open(name, scno, true);
	jsonl_arg_open(0, "sockfd", side);
	jsonl_fd(fd, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "level", side);
	jsonl_const(SOL_SOCKET, true, "SOL_SOCKET", NULL);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "optname", side);
	jsonl_const(optname, true, optname_sym, NULL);
	jsonl_arg_close(side);
}

static void
print_ld_b_abs_code(void)
{
	printf("\"code\": {");
	jsonl_flags_groups_open(BPF_LD | BPF_B | BPF_ABS);
	printf("{\"raw\": \"0\", \"group_type\": \"flags\", \"elems\": [{");
	jsonl_const(BPF_LD, true, "BPF_LD", NULL);
	printf("}]}, {\"raw\": \"0x10\", \"group_type\": \"flags\","
	       " \"elems\": [{");
	jsonl_const(BPF_B, true, "BPF_B", NULL);
	printf("}]}, {\"raw\": \"0x20\", \"group_type\": \"flags\","
	       " \"elems\": [{");
	jsonl_const(BPF_ABS, true, "BPF_ABS", NULL);
	printf("}]}");
	jsonl_flags_groups_close();
	printf("}");
}

static void
print_skf_off(unsigned int base, const char *base_sym, unsigned int offset,
	      const char *offset_sym, bool last)
{
	printf("{\"type\": \"bpf_insn\", \"kind\": \"stmt\", ");
	print_ld_b_abs_code();
	printf(", \"k\": {\"type\": \"skf_off\", \"raw\": \"%#x\","
	       " \"base\": {", base + offset);
	jsonl_const(base, true, base_sym, NULL);
	printf("}, \"offset\": {");
	if (offset_sym)
		jsonl_const(offset, true, offset_sym, NULL);
	else
		jsonl_unsigned(offset);
	printf("}}}%s", last ? "" : ", ");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		perror_msg_and_skip("socket AF_INET");

	TAIL_ALLOC_OBJECT_CONST_PTR(int, plen);
	*plen = 4096;
	long rc = syscall(__NR_getsockopt, fd, SOL_SOCKET, SO_GET_FILTER,
			  NULL, plen);
	if (rc)
		perror_msg_and_skip("getsockopt SO_GET_FILTER");

	open_sockopt("getsockopt", __NR_getsockopt, fd, SO_GET_FILTER,
		     "SO_GET_FILTER");
	printf(", ");
	if (merged) {
		jsonl_arg_open(3, "optval", "exit");
		jsonl_addr(NULL);
		jsonl_arg_close("exit");
		printf(", {\"index\": 4, \"arg\": \"optlen\", \"entry\": {");
		jsonl_decimal(4096);
		printf(", \"indirect\": true, \"inout\": true}, \"exit\": {");
	} else {
		jsonl_arg_open(4, "optlen", NULL);
		jsonl_decimal(4096);
		printf(", \"indirect\": true, \"inout\": true}]}\n");
		jsonl_syscall_open("getsockopt", __NR_getsockopt, false);
		jsonl_arg_open(3, "optval", NULL);
		jsonl_addr(NULL);
		jsonl_arg_close(NULL);
		printf(", ");
		jsonl_arg_open(4, "optlen", NULL);
	}
	jsonl_decimal(*plen);
	printf(", \"indirect\": true%s",
	       *plen != 4096 ? ", \"changed\": true" : "");
	jsonl_arg_close(merged ? "exit" : NULL);
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");

	static const struct sock_filter insns[] = {
		BPF_STMT(BPF_LD | BPF_B | BPF_ABS, SKF_LL_OFF + 4),
		BPF_STMT(BPF_LD | BPF_B | BPF_ABS, SKF_NET_OFF + 8),
		BPF_STMT(BPF_LD | BPF_B | BPF_ABS,
			 SKF_AD_OFF + SKF_AD_PROTOCOL),
	};
	TAIL_ALLOC_OBJECT_CONST_PTR(struct sock_fprog, prog);
	prog->len = ARRAY_SIZE(insns);
	prog->filter = tail_memdup(insns, sizeof(insns));

	rc = syscall(__NR_setsockopt, fd, SOL_SOCKET, SO_ATTACH_FILTER,
		     prog, sizeof(*prog));

	const char *const side = merged ? "entry" : NULL;
	open_sockopt("setsockopt", __NR_setsockopt, fd, SO_ATTACH_FILTER,
		     "SO_ATTACH_FILTER");
	printf(", ");
	jsonl_arg_open(3, "optval", side);
	jsonl_struct_open();
	jsonl_field_unsigned("len", true, prog->len);
	printf(", \"filter\": {");
	jsonl_array_open();
	print_skf_off(SKF_LL_OFF, "SKF_LL_OFF", 4, NULL, false);
	print_skf_off(SKF_NET_OFF, "SKF_NET_OFF", 8, NULL, false);
	print_skf_off(SKF_AD_OFF, "SKF_AD_OFF", SKF_AD_PROTOCOL,
		      "SKF_AD_PROTOCOL", true);
	jsonl_array_close();
	printf("}");
	jsonl_struct_close();
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(4, "optlen", side);
	jsonl_decimal(sizeof(*prog));
	jsonl_arg_close(side);
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("setsockopt", __NR_setsockopt, false);
	}
	printf("], ");
	jsonl_return("unsigned", rc);
	printf("}\n");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_setsockopt && __NR_getsockopt")

#endif
