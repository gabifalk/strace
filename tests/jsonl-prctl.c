/*
 * Check JSONL output of prctl composed values and return values:
 * a flags return value, composed arguments of PR_SVE_SET_VL and
 * PR_SET_TAGGED_ADDR_CTRL, and composed return values of PR_SVE_*_VL
 * and PR_RISCV_V_GET_CONTROL.  The test is run with every prctl call
 * returning an injected RETVAL.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_prctl

# include "jsonl.h"

# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <linux/prctl.h>

# define RETVAL 0x20005

static bool merged;
static const char *side;

static void
open_event(unsigned int op, const char *op_sym)
{
	if (merged)
		jsonl_syscall_merged_open("prctl", __NR_prctl);
	else
		jsonl_syscall_open("prctl", __NR_prctl, true);
	jsonl_arg_open(0, "op", side);
	jsonl_const(op, true, op_sym, NULL);
	jsonl_arg_close(side);
}

static void
open_return(void)
{
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("prctl", __NR_prctl, false);
	}
	printf("], \"return\": {");
}

static void
close_return(void)
{
	printf(", \"injected\": true}}\n");
}

static void
print_hex_elem(unsigned long long val)
{
	printf(", {");
	jsonl_hex(val);
	printf("}");
}

/* RETVAL as a vector length value: PR_SVE_VL_INHERIT|0x5. */
static void
print_sve_retval(void)
{
	jsonl_flags_open(RETVAL);
	jsonl_flags_const(PR_SVE_VL_INHERIT, "PR_SVE_VL_INHERIT", true);
	print_hex_elem(RETVAL & PR_SVE_VL_LEN_MASK);
	jsonl_flags_close();
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	side = merged ? "entry" : NULL;

	/* A flags return value: SECBIT_NOROOT|SECBIT_NO_SETUID_FIXUP|0x20000 */
	syscall(__NR_prctl, PR_GET_SECUREBITS, 0, 0, 0, 0);
	open_event(PR_GET_SECUREBITS, "PR_GET_SECUREBITS");
	open_return();
	jsonl_flags_open_dflt_null(RETVAL);
	jsonl_flags_const(1 << 0, "SECBIT_NOROOT", true);
	jsonl_flags_const(1 << 2, "SECBIT_NO_SETUID_FIXUP", false);
	jsonl_flags_remainder(0x20000, NULL, false);
	jsonl_flags_close();
	close_return();

	/* A composed argument and a composed return value. */
	const unsigned long sve_arg = PR_SVE_SET_VL_ONEXEC | 0xff;
	syscall(__NR_prctl, PR_SVE_SET_VL, sve_arg, 0, 0, 0);
	open_event(PR_SVE_SET_VL, "PR_SVE_SET_VL");
	printf(", ");
	jsonl_arg_open(1, "value", side);
	jsonl_flags_open(sve_arg);
	jsonl_flags_const(PR_SVE_SET_VL_ONEXEC, "PR_SVE_SET_VL_ONEXEC", true);
	print_hex_elem(0xff);
	jsonl_flags_close();
	jsonl_arg_close(side);
	open_return();
	print_sve_retval();
	close_return();

	syscall(__NR_prctl, PR_SVE_GET_VL, 0, 0, 0, 0);
	open_event(PR_SVE_GET_VL, "PR_SVE_GET_VL");
	open_return();
	print_sve_retval();
	close_return();

	/* A composed argument with a hexadecimal shifted part. */
	const unsigned long tag_arg = PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC
				      | (0x2a << PR_MTE_TAG_SHIFT);
	syscall(__NR_prctl, PR_SET_TAGGED_ADDR_CTRL, tag_arg, 0, 0, 0);
	open_event(PR_SET_TAGGED_ADDR_CTRL, "PR_SET_TAGGED_ADDR_CTRL");
	printf(", ");
	jsonl_arg_open(1, "mode", side);
	jsonl_flags_open(tag_arg);
	jsonl_flags_const(PR_TAGGED_ADDR_ENABLE, "PR_TAGGED_ADDR_ENABLE", true);
	jsonl_flags_const(PR_MTE_TCF_SYNC, "PR_MTE_TCF_SYNC", false);
	printf(", {\"type\": \"shift\", \"raw\": ");
	jsonl_xlat_raw(0x2a << PR_MTE_TAG_SHIFT);
	printf(", \"value\": {");
	jsonl_hex(0x2a);
	printf("}, \"shift\": \"PR_MTE_TAG_SHIFT\"}");
	jsonl_flags_close();
	jsonl_arg_close(side);
	for (unsigned int i = 2; i < 5; ++i) {
		const char name[] = { 'a', 'r', 'g', '1' + i, '\0' };
		printf(", ");
		jsonl_arg_open(i, name, side);
		jsonl_hex(0);
		jsonl_arg_close(side);
	}
	open_return();
	jsonl_unsigned(RETVAL);
	close_return();

	/*
	 * A composed return value with a constant shifted part:
	 * PR_RISCV_V_VSTATE_CTRL_OFF|PR_RISCV_V_VSTATE_CTRL_OFF<<2
	 * |!PR_RISCV_V_VSTATE_CTRL_INHERIT|0x20000
	 */
	syscall(__NR_prctl, PR_RISCV_V_GET_CONTROL, 0, 0, 0, 0);
	open_event(PR_RISCV_V_GET_CONTROL, "PR_RISCV_V_GET_CONTROL");
	open_return();
	jsonl_flags_open(RETVAL);
	jsonl_flags_const(PR_RISCV_V_VSTATE_CTRL_OFF,
			  "PR_RISCV_V_VSTATE_CTRL_OFF", true);
	printf(", {\"type\": \"shift\", \"raw\": ");
	jsonl_xlat_raw(PR_RISCV_V_VSTATE_CTRL_OFF << 2);
	printf(", \"value\": {");
	jsonl_const(PR_RISCV_V_VSTATE_CTRL_OFF, true,
		    "PR_RISCV_V_VSTATE_CTRL_OFF", NULL);
	printf("}, \"shift\": \"2\"}");
	jsonl_flags_const(0, "!PR_RISCV_V_VSTATE_CTRL_INHERIT", false);
	print_hex_elem(0x20000);
	jsonl_flags_close();
	close_return();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_prctl")

#endif
