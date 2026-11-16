/*
 * Copyright (c) 1994-1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-2000 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 2005-2007 Roland McGrath <roland@redhat.com>
 * Copyright (c) 2008-2015 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2014-2025 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"
#include "xstring.h"

#include <linux/prctl.h>

#include "xlat/prctl_options.h"
#include "xlat/pr_cap_ambient.h"
#include "xlat/pr_dumpable.h"
#include "xlat/pr_fp_mode.h"
#include "xlat/pr_mce_kill.h"
#include "xlat/pr_mce_kill_policy.h"
#include "xlat/pr_mdwe_flags.h"
#include "xlat/pr_pac_enabled_keys.h"
#include "xlat/pr_pac_keys.h"
#include "xlat/pr_ppc_dexcr_aspects.h"
#include "xlat/pr_ppc_dexcr_ctrl_flags.h"
#include "xlat/pr_riscv_icache_flush_ctxes.h"
#include "xlat/pr_riscv_icache_flush_scopes.h"
#include "xlat/pr_riscv_v_inherit.h"
#include "xlat/pr_riscv_v_states.h"
#include "xlat/pr_sched_core_cmds.h"
#include "xlat/pr_sched_core_pidtypes.h"
#include "xlat/pr_set_mm.h"
#include "xlat/pr_set_vma.h"
#include "xlat/pr_sme_vl_flags.h"
#include "xlat/pr_spec_cmds.h"
#include "xlat/pr_spec_get_store_bypass_flags.h"
#include "xlat/pr_spec_set_store_bypass_flags.h"
#include "xlat/pr_sud_cmds.h"
#include "xlat/pr_sve_vl_flags.h"
#include "xlat/pr_tagged_addr_enable.h"
#include "xlat/pr_tagged_addr_mte_tcf.h"
#include "xlat/pr_tsc.h"
#include "xlat/pr_unalign_flags.h"

#ifndef TASK_COMM_LEN
# define TASK_COMM_LEN 16
#endif

#include <linux/seccomp.h>
#include "xlat/seccomp_mode.h"

#include <linux/securebits.h>
#include "xlat/secbits.h"

/* these constants are the same as in <linux/capability.h> */
enum {
#include "caps0.h"
#include "caps1.h"
};

#include "xlat/cap.h"

static void
print_prctl_args(struct tcb *tcp, const unsigned int first)
{
	for (unsigned int i = first; i < n_args(tcp); ++i) {
		const char name[] = { 'a', 'r', 'g', "123456789"[i], '\0' };
		tprints_arg_next_name(name);
		PRINT_VAL_X(tcp->u_arg[i]);
	}
}

static void
print_vl_val(const unsigned long long arg, const enum xlat_style style,
	     const unsigned long long len_mask, const struct xlat *xlat)
{
	const unsigned long long vl = arg & len_mask;
	const unsigned long long flags = arg & ~len_mask;

	if (!flags) {
		PRINT_VAL_X(arg);
	} else if (tprint_xlat_flags_value_begin(arg, style)) {
		printflags_ex(flags, NULL, XLAT_STYLE_ABBREV, xlat, NULL);
		tprint_flags_value_or();
		PRINT_VAL_X(vl);
		tprint_xlat_flags_value_end(style);
	}
}

static void
print_sve_val(const unsigned long long arg, const enum xlat_style style)
{
	print_vl_val(arg, style, PR_SVE_VL_LEN_MASK, pr_sve_vl_flags);
}

static void
print_sme_val(const unsigned long long arg, const enum xlat_style style)
{
	print_vl_val(arg, style, PR_SME_VL_LEN_MASK, pr_sme_vl_flags);
}

static void
print_tagged_addr_val(const unsigned long long arg, const enum xlat_style style)
{
	const unsigned long long enabled = arg & PR_TAGGED_ADDR_ENABLE;
	const unsigned long long mte_tcf = arg & PR_MTE_TCF_MASK;
	const unsigned long long mte_tag = arg & PR_MTE_TAG_MASK;
	const unsigned long long rest = arg &
		~((unsigned long long) PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_MASK
		  | PR_MTE_TAG_MASK);

	if (!tprint_xlat_flags_value_begin(arg, style))
		return;

	printflags_ex(enabled, NULL, XLAT_STYLE_ABBREV,
		      pr_tagged_addr_enable, NULL);
	tprint_flags_value_or();
	printxvals_ex(mte_tcf, NULL, XLAT_STYLE_ABBREV,
		      pr_tagged_addr_mte_tcf, NULL);
	if (mte_tag) {
		tprint_flags_value_or();
		tprint_shift_value_begin(mte_tag);
		PRINT_VAL_X(mte_tag >> PR_MTE_TAG_SHIFT);
		tprints_shift_value_end("PR_MTE_TAG_SHIFT");
	}
	if (rest) {
		tprint_flags_value_or();
		PRINT_VAL_X(rest);
	}

	tprint_xlat_flags_value_end(style);
}

static void
print_riscv_v_ctrl_val(const unsigned long long arg,
		       const enum xlat_style style)
{
	const unsigned long long inherit = arg & PR_RISCV_V_VSTATE_CTRL_INHERIT;
	const unsigned long long next_bits =
		arg & PR_RISCV_V_VSTATE_CTRL_NEXT_MASK;
	const unsigned long long cur = arg & PR_RISCV_V_VSTATE_CTRL_CUR_MASK;
	const unsigned long long rest =
		arg & ~((unsigned long long) PR_RISCV_V_VSTATE_CTRL_MASK);

	if (!tprint_xlat_flags_value_begin(arg, style))
		return;

	printxvals_ex(cur, NULL, XLAT_STYLE_ABBREV, pr_riscv_v_states, NULL);
	tprint_flags_value_or();
	tprint_shift_value_begin(next_bits);
	printxvals_ex(next_bits >> 2, NULL, XLAT_STYLE_ABBREV,
		      pr_riscv_v_states, NULL);
	tprint_shift_value_end_u(2);
	tprint_flags_value_or();
	printflags_ex(inherit, NULL, XLAT_STYLE_ABBREV,
		      pr_riscv_v_inherit, NULL);
	if (rest) {
		tprint_flags_value_or();
		PRINT_VAL_X(rest);
	}

	tprint_xlat_flags_value_end(style);
}

static int
print_get_uint_arg(struct tcb *tcp, const kernel_ulong_t addr, const char *name)
{
	if (entering(tcp))
		return 0;

	tprints_arg_next_name(name);
	printnum_int_u(tcp, addr);

	return RVAL_DECODED;
}

static int
print_set_kulong_arg(struct tcb *tcp, const kernel_ulong_t arg, const char *name,
		     const unsigned int check_first)
{
	tprints_arg_next_name(name);
	PRINT_VAL_U(arg);
	if (check_first)
		print_prctl_args(tcp, check_first);

	return RVAL_DECODED;
}

SYS_FUNC(prctl)
{
	const unsigned int option = tcp->u_arg[0];
	const kernel_ulong_t arg2 = tcp->u_arg[1];
	const kernel_ulong_t arg3 = tcp->u_arg[2];
	const kernel_ulong_t arg4 = tcp->u_arg[3];
	const kernel_ulong_t arg5 = tcp->u_arg[4];
	unsigned int i;

	if (entering(tcp)) {
		tprints_arg_name("op");
		printxval(prctl_options, option, "PR_???");
	}

	/*
	 * Grouped options with common decoders first (except for the options
	 * that are decoded the same as the default case, that grouped together
	 * with it), then sorted according to the option number.
	 */
	switch (option) {
	/* Group 1: no args */
	case PR_GET_KEEPCAPS:			/*   7 */
	case PR_GET_TIMING:			/*  13 */
	case PR_GET_SECCOMP:			/*  21 */
	case PR_GET_TIMERSLACK:			/*  30 */
	case PR_TASK_PERF_EVENTS_DISABLE:	/*  31 */
	case PR_TASK_PERF_EVENTS_ENABLE:	/*  32 */
		return RVAL_DECODED;

	/* Group 2: getter with the first options as a uint pointer */
	case PR_GET_FPEMU:			/*   9 */
		return print_get_uint_arg(tcp, arg2, "fpemu");
	case PR_GET_FPEXC:			/*  11 */
		return print_get_uint_arg(tcp, arg2, "mode");
	case PR_GET_ENDIAN:			/*  19 */
		return print_get_uint_arg(tcp, arg2, "endianness");
	case PR_GET_CHILD_SUBREAPER:		/*  37 */
		return print_get_uint_arg(tcp, arg2, "isset");

	/* Group 3: one kulong argument */
	case PR_SET_KEEPCAPS:			/*   8 */
		return print_set_kulong_arg(tcp, arg2, "state", 0);
	case PR_SET_FPEMU:			/*  10 */
		return print_set_kulong_arg(tcp, arg2, "fpemu", 0);
	case PR_SET_FPEXC:			/*  12 */
	case PR_SET_TIMING:			/*  14 */
		return print_set_kulong_arg(tcp, arg2, "mode", 0);
	case PR_SET_ENDIAN:			/*  20 */
		return print_set_kulong_arg(tcp, arg2, "endianness", 0);
	case PR_SET_CHILD_SUBREAPER:		/*  36 */
		return print_set_kulong_arg(tcp, arg2, "set", 0);

	/* Group 4: one uint option and zero check for the rest */
	case PR_SET_NO_NEW_PRIVS:		/*  38 */
		return print_set_kulong_arg(tcp, arg2, "set", 2);
	case PR_SET_THP_DISABLE:		/*  41 */
		return print_set_kulong_arg(tcp, arg2, "flag", 2);
	case PR_SET_IO_FLUSHER:			/*  57 */
		return print_set_kulong_arg(tcp, arg2, "state", 2);

	case PR_GET_PDEATHSIG:			/*   1 */
		if (entering(tcp))
			break;
		tprints_arg_next_name("sig");
		if (!umove_or_printaddr(tcp, arg2, &i)) {
			tprint_indirect_begin();
			printsignal(i);
			tprint_indirect_end();
		}
		break;

	case PR_SET_PDEATHSIG:			/*   2 */
		tprints_arg_next_name("sig");
		if (arg2 > 128)
			PRINT_VAL_U(arg2);
		else
			printsignal(arg2);
		return RVAL_DECODED;

	case PR_GET_DUMPABLE:			/*   3 */
		if (entering(tcp))
			break;
		if (syserror(tcp))
			return 0;
		tcp->rval_xlat = pr_dumpable;
		return RVAL_XLAT;

	case PR_SET_DUMPABLE:			/*   4 */
		tprints_arg_next_name("dumpable");
		printxval64(pr_dumpable, arg2, "SUID_DUMP_???");
		return RVAL_DECODED;

	case PR_GET_UNALIGN:			/*   5 */
		if (entering(tcp))
			break;
		tprints_arg_next_name("flags");
		if (!umove_or_printaddr(tcp, arg2, &i)) {
			tprint_indirect_begin();
			printflags(pr_unalign_flags, i, "PR_UNALIGN_???");
			tprint_indirect_end();
		}
		break;

	case PR_SET_UNALIGN:			/*   6 */
		tprints_arg_next_name("flags");
		printflags(pr_unalign_flags, arg2, "PR_UNALIGN_???");
		return RVAL_DECODED;

	/* PR_GET_KEEPCAPS - group 1		     7 */
	/* PR_SET_KEEPCAPS - group 3		     8 */
	/* PR_GET_FPEMU - group 2		     9 */
	/* PR_SET_FPEMU - group 3		    10 */
	/* PR_GET_FPEXC - group 2		    11 */
	/* PR_SET_FPEXC - group 3		    12 */
	/* PR_GET_TIMING - group 1		    13 */
	/* PR_SET_TIMING - group 3		    14 */

	case PR_SET_NAME:			/*  15 */
		tprints_arg_next_name("name");
		printstr_ex(tcp, arg2, TASK_COMM_LEN - 1,
			    QUOTE_0_TERMINATED);
		return RVAL_DECODED;

	case PR_GET_NAME:			/*  16 */
		if (entering(tcp))
			break;
		tprints_arg_next_name("name");
		if (syserror(tcp))
			printaddr(arg2);
		else
			printstr_ex(tcp, arg2, TASK_COMM_LEN,
				    QUOTE_0_TERMINATED);
		break;

	/* There are no options with numbers 17 and 18 */
	/* PR_GET_ENDIAN - group 1		    19 */
	/* PR_SET_ENDIAN - group 3		    20 */
	/* PR_GET_SECCOMP - group 1		    21 */

	case PR_SET_SECCOMP:			/*  22 */
		tprints_arg_next_name("mode");
		printxval64(seccomp_mode, arg2,
			    "SECCOMP_MODE_???");
		if (SECCOMP_MODE_STRICT == arg2)
			return RVAL_DECODED;
		if (SECCOMP_MODE_FILTER == arg2) {
			tprints_arg_next_name("filter");
			decode_seccomp_fprog(tcp, arg3);
			return RVAL_DECODED;
		}
		print_prctl_args(tcp, 2);
		return RVAL_DECODED;

	case PR_CAPBSET_READ:			/*  23 */
	case PR_CAPBSET_DROP:			/*  24 */
		tprints_arg_next_name("cap");
		printxval64(cap, arg2, "CAP_???");
		return RVAL_DECODED;

	case PR_GET_TSC:			/*  25 */
		if (entering(tcp))
			break;
		tprints_arg_next_name("flag");
		if (!umove_or_printaddr(tcp, arg2, &i)) {
			tprint_indirect_begin();
			printxval(pr_tsc, i, "PR_TSC_???");
			tprint_indirect_end();
		}
		break;

	case PR_SET_TSC:			/*  26 */
		tprints_arg_next_name("flag");
		printxval(pr_tsc, arg2, "PR_TSC_???");
		return RVAL_DECODED;

	case PR_GET_SECUREBITS:			/*  27 */
		if (entering(tcp))
			break;
		if (syserror(tcp) || tcp->u_rval == 0)
			return 0;
		tcp->rval_xlat = secbits;
		return RVAL_FLAGS;

	case PR_SET_SECUREBITS:			/*  28 */
		tprints_arg_next_name("flags");
		printflags64(secbits, arg2, "SECBIT_???");
		return RVAL_DECODED;

	case PR_SET_TIMERSLACK:			/*  29 */
		tprints_arg_next_name("slack");
		PRINT_VAL_D(arg2);
		return RVAL_DECODED;

	/* PR_GET_TIMERSLACK - group 1		    30 */
	/* PR_TASK_PERF_EVENTS_DISABLE - group 1    31 */
	/* PR_TASK_PERF_EVENTS_ENABLE - group 1	    32 */

	case PR_MCE_KILL:			/*  33 */
		tprints_arg_next_name("operation");
		printxval64(pr_mce_kill, arg2, "PR_MCE_KILL_???");
		if (PR_MCE_KILL_SET == arg2) {
			tprints_arg_next_name("policy");
			printxval64(pr_mce_kill_policy, arg3,
				    "PR_MCE_KILL_???");
			print_prctl_args(tcp, 3);
		} else {
			print_prctl_args(tcp, 2);
		}
		return RVAL_DECODED;

	case PR_MCE_KILL_GET:			/*  34 */
		if (entering(tcp)) {
			print_prctl_args(tcp, 1);
			return 0;
		}
		if (syserror(tcp))
			return 0;
		tcp->rval_xlat = pr_mce_kill_policy;
		return RVAL_XLAT;

	case PR_SET_MM:				/*  35 */
		tprints_arg_next_name("operation");
		printxval(pr_set_mm, arg2, "PR_SET_MM_???");
		print_prctl_args(tcp, 2);
		return RVAL_DECODED;

	/* PR_SET_CHILD_SUBREAPER - group 3	    36 */
	/* PR_GET_CHILD_SUBREAPER - group 2	    37 */
	/* PR_SET_NO_NEW_PRIVS - group 4	    38 */
	/* PR_GET_NO_NEW_PRIVS - group 5	    39 */

	case PR_GET_TID_ADDRESS:		/*  40 */
		if (entering(tcp))
			break;
		tprints_arg_next_name("addr");
		printnum_kptr(tcp, arg2);
		break;

	/* PR_SET_THP_DISABLE - group 4		    41 */
	/* PR_GET_THP_DISABLE - group 5		    42 */
	/* PR_MPX_ENABLE_MANAGEMENT - group 5	    43 */
	/* PR_MPX_DISABLE_MANAGEMENT - group 5	    44 */

	case PR_SET_FP_MODE:			/*  45 */
		tprints_arg_next_name("mode");
		printflags(pr_fp_mode, arg2, "PR_FP_MODE_???");
		return RVAL_DECODED;

	case PR_GET_FP_MODE:			/*  46 */
		if (entering(tcp))
			break;
		if (syserror(tcp) || tcp->u_rval == 0)
			return 0;
		tcp->rval_xlat = pr_fp_mode;
		return RVAL_FLAGS;

	case PR_CAP_AMBIENT:			/*  47 */
		tprints_arg_next_name("operation");
		printxval64(pr_cap_ambient, arg2,
			       "PR_CAP_AMBIENT_???");
		switch (arg2) {
		case PR_CAP_AMBIENT_RAISE:
		case PR_CAP_AMBIENT_LOWER:
		case PR_CAP_AMBIENT_IS_SET:
			tprints_arg_next_name("caps");
			printxval64(cap, arg3, "CAP_???");
			print_prctl_args(tcp, 3);
			break;
		default:
			print_prctl_args(tcp, 2);
			break;
		}
		return RVAL_DECODED;

	/* There are no options with numbers 48 and 49 */

	case PR_SVE_SET_VL:			/*  50 */
		if (entering(tcp)) {
			tprints_arg_next_name("value");
			print_sve_val(arg2, XLAT_STYLE_DEFAULT);
			return 0;
		}
		ATTRIBUTE_FALLTHROUGH;

	case PR_SVE_GET_VL:			/*  51 */
		if (entering(tcp))
			break;
		if (syserror(tcp) || tcp->u_rval == 0)
			return 0;

		if (!(tcp->u_rval & ~PR_SVE_VL_LEN_MASK))
			return RVAL_HEX;

		tcp->rval_printer = print_sve_val;

		return RVAL_HEX | RVAL_PRINTER;

	case PR_GET_SPECULATION_CTRL:		/*  52 */
		if (entering(tcp)) {
			tprints_arg_next_name("misfeature");
			printxval64(pr_spec_cmds, arg2, "PR_SPEC_???");

			break;
		}

		if (syserror(tcp))
			return 0;

		switch (arg2) {
		case PR_SPEC_STORE_BYPASS:
		case PR_SPEC_INDIRECT_BRANCH:
		case PR_SPEC_L1D_FLUSH:
			tcp->rval_xlat = pr_spec_get_store_bypass_flags;
			return RVAL_FLAGS;
		}

		return 0;

	case PR_SET_SPECULATION_CTRL:		/*  53 */
		tprints_arg_next_name("misfeature");
		printxval64(pr_spec_cmds, arg2, "PR_SPEC_???");

		tprints_arg_next_name("value");
		switch (arg2) {
		case PR_SPEC_STORE_BYPASS:
		case PR_SPEC_INDIRECT_BRANCH:
		case PR_SPEC_L1D_FLUSH:
			printxval64(pr_spec_set_store_bypass_flags, arg3,
				    "PR_SPEC_???");
			break;

		default:
			PRINT_VAL_X(arg3);
		}

		return RVAL_DECODED;

	case PR_PAC_RESET_KEYS:			/*  54 */
		tprints_arg_next_name("keys");
		printflags_ex(arg2, "PR_PAC_???", XLAT_STYLE_DEFAULT,
			      pr_pac_enabled_keys, pr_pac_keys, NULL);
		print_prctl_args(tcp, 2);

		return RVAL_DECODED;

	case PR_SET_TAGGED_ADDR_CTRL:		/*  55 */
		tprints_arg_next_name("mode");
		print_tagged_addr_val(arg2, XLAT_STYLE_DEFAULT);
		print_prctl_args(tcp, 2);
		return RVAL_DECODED;

	case PR_GET_TAGGED_ADDR_CTRL:		/*  56 */
		if (entering(tcp)) {
			print_prctl_args(tcp, 1);
			break;
		}
		if (syserror(tcp))
			return 0;
		tcp->rval_printer = print_tagged_addr_val;

		return RVAL_HEX | RVAL_PRINTER;

	/* PR_SET_IO_FLUSHER - group 4		    57 */
	/* PR_GET_IO_FLUSHER - group 5		    58 */

	case PR_SET_SYSCALL_USER_DISPATCH:	/*  59 */
		tprints_arg_next_name("operation");
		printxval64(pr_sud_cmds, arg2, "PR_SYS_DISPATCH_???");
		tprints_arg_next_name("off");
		PRINT_VAL_X(arg3);
		tprints_arg_next_name("size");
		PRINT_VAL_X(arg4);
		tprints_arg_next_name("switch");
		printaddr(arg5);

		return RVAL_DECODED;

	case PR_PAC_SET_ENABLED_KEYS:		/*  60 */
		tprints_arg_next_name("bitmask_affected");
		printflags64(pr_pac_enabled_keys, arg2, "PR_PAC_???");
		tprints_arg_next_name("bitmask_set");
		printflags64(pr_pac_enabled_keys, arg3, "PR_PAC_???");
		print_prctl_args(tcp, 3);

		return RVAL_DECODED;

	case PR_PAC_GET_ENABLED_KEYS:		/*  61 */
		if (entering(tcp)) {
			print_prctl_args(tcp, 1);
			return 0;
		}
		if (syserror(tcp))
			return 0;
		tcp->rval_xlat = pr_pac_enabled_keys;
		return RVAL_FLAGS;

	case PR_SCHED_CORE:			/*  62 */
		if (entering(tcp)) {
			tprints_arg_next_name("operation");
			printxval(pr_sched_core_cmds, arg2, "PR_SCHED_CORE_???");

			tprints_arg_next_name("pid");
			enum pid_type pt;
			switch ((unsigned int) arg4) {
			case PIDTYPE_PID:  pt = PT_TID;  break;
			case PIDTYPE_TGID: pt = PT_TGID; break;
			case PIDTYPE_PGID: pt = PT_PGID; break;
			case PIDTYPE_SID:  pt = PT_SID;  break;
			default:           pt = PT_NONE;
			}
			printpid(tcp, arg3, pt);

			tprints_arg_next_name("pid_type");
			printxval_ex(pr_sched_core_pidtypes,
				     (unsigned int) arg4, "PIDTYPE_???",
				     (xlat_verbose(xlat_verbosity)
							== XLAT_STYLE_RAW)
					? XLAT_STYLE_DEFAULT
					: XLAT_STYLE_VERBOSE);

			switch ((unsigned int) arg2) {
			case PR_SCHED_CORE_GET:
				/* arg5 is to be decoded on exiting */
				return 0;
			default:
				print_prctl_args(tcp, 4);
			}
		} else {
			/* PR_SCHED_CORE_GET */
			tprints_arg_next_name("cookie");
			if (syserror(tcp))
				printaddr(arg5);
			else
				printnum_int64_x(tcp, arg5);
		}

		return RVAL_DECODED;

	case PR_SME_SET_VL:			/*  63 */
		if (entering(tcp)) {
			tprints_arg_next_name("arg");
			print_sme_val(arg2, XLAT_STYLE_DEFAULT);
			return 0;
		}
		ATTRIBUTE_FALLTHROUGH;

	case PR_SME_GET_VL:			/*  64 */
		if (entering(tcp))
			break;
		if (syserror(tcp) || tcp->u_rval == 0)
			return 0;

		if (!(tcp->u_rval & ~PR_SME_VL_LEN_MASK))
			return RVAL_HEX;

		tcp->rval_printer = print_sme_val;

		return RVAL_HEX | RVAL_PRINTER;

	case PR_SET_MDWE:			/*  65 */
		tprints_arg_next_name("mask");
		printflags(pr_mdwe_flags, arg2, "PR_MDWE_???");
		print_prctl_args(tcp, 2);
		return RVAL_DECODED;

	case PR_GET_MDWE:			/*  66 */
		if (entering(tcp)) {
			print_prctl_args(tcp, 1);
			break;
		}
		if (syserror(tcp))
			return 0;
		tcp->rval_xlat = pr_mdwe_flags;
		return RVAL_FLAGS;

	/* PR_SET_MEMORY_MERGE - group 5	    67 */
	/* PR_GET_MEMORY_MERGE - group 5	    68 */

	case PR_RISCV_V_SET_CONTROL:		/*  69 */
		tprints_arg_next_name("arg");
		print_riscv_v_ctrl_val(arg2, XLAT_STYLE_DEFAULT);
		/*
		 * PR_RISCV_V_[GS]ET_CONTROL are modern options
		 * that do not check unused arguments for being zero.
		 */
		return RVAL_DECODED;

	case PR_RISCV_V_GET_CONTROL:		/*  70 */
		if (entering(tcp))
			break;
		if (syserror(tcp))
			return 0;
		tcp->rval_printer = print_riscv_v_ctrl_val;

		return RVAL_HEX | RVAL_PRINTER;

	case PR_RISCV_SET_ICACHE_FLUSH_CTX:	/*  71 */
		tprints_arg_next_name("ctx");
		printxval64(pr_riscv_icache_flush_ctxes, arg2,
			    "PR_RISCV_CTX_???");
		tprints_arg_next_name("scope");
		printxval64(pr_riscv_icache_flush_scopes, arg3,
			    "PR_RISCV_SCOPE_???");
		/*
		 * PR_RISCV_SET_ICACHE_FLUSH_CTX is a modern option
		 * that does not check unused arguments for being zero.
		 */
		return RVAL_DECODED;

	case PR_PPC_GET_DEXCR:			/*  72 */
		if (entering(tcp)) {
			tprints_arg_next_name("which");
			printxval64(pr_ppc_dexcr_aspects, arg2,
				    "PR_PPC_DEXCR_???");
			print_prctl_args(tcp, 2);
		}
		if (syserror(tcp) || tcp->u_rval == 0)
			return 0;
		tcp->rval_xlat = pr_ppc_dexcr_ctrl_flags;
		return RVAL_FLAGS;

	case PR_PPC_SET_DEXCR:			/*  73 */
		tprints_arg_next_name("which");
		printxval64(pr_ppc_dexcr_aspects, arg2, "PR_PPC_DEXCR_???");
		tprints_arg_next_name("ctrl");
		printflags64(pr_ppc_dexcr_ctrl_flags, arg3,
			     "PR_PPC_DEXCR_CTRL_???");
		print_prctl_args(tcp, 3);
		return RVAL_DECODED;

	case PR_SET_VMA:			/* 0x53564d41 */
		tprints_arg_next_name("attr");
		printxval64(pr_set_vma, arg2, "PR_SET_VMA_???");
		if (arg2 == PR_SET_VMA_ANON_NAME) {
			tprints_arg_next_name("addr");
			printaddr(arg3);
			tprints_arg_next_name("size");
			PRINT_VAL_U(arg4);
			tprints_arg_next_name("name");
			printstr(tcp, arg5);
		} else {
			/* There are no other sub-options now, but there
			 * might be in future... */
			print_prctl_args(tcp, 2);
		}
		return RVAL_DECODED;

	case PR_SET_PTRACER:			/* 0x59616d61 */
		tprints_arg_next_name("pid");
		if ((int) arg2 == -1) {
			print_xlat_ex((int) arg2, "PR_SET_PTRACER_ANY",
				      XLAT_STYLE_FMT_D);
		} else {
			printpid(tcp, arg2, PT_TGID);
		}
		return RVAL_DECODED;

	/* Group 5: default decoding */
	case PR_GET_NO_NEW_PRIVS:		/*  39 */
	case PR_GET_THP_DISABLE:		/*  42 */
	case PR_MPX_ENABLE_MANAGEMENT:		/*  43 */
	case PR_MPX_DISABLE_MANAGEMENT:		/*  44 */
	case PR_GET_IO_FLUSHER:			/*  58 */
	case PR_SET_MEMORY_MERGE:		/*  67 */
	case PR_GET_MEMORY_MERGE:		/*  68 */
	default:
		print_prctl_args(tcp, 1);
		return RVAL_DECODED;
	}
	return 0;
}

#if defined X86_64 || defined X32 || defined I386
# include "xlat/archvals.h"
# include "xlat/x86_xfeature_bits.h"
# include "xlat/x86_xfeatures.h"

SYS_FUNC(arch_prctl)
{
	const unsigned int option = tcp->u_arg[0];
	const kernel_ulong_t addr = tcp->u_arg[1];

	if (entering(tcp)) {
		tprints_arg_name("op");
		printxval(archvals, option, "ARCH_???");
	}

	switch (option) {
	case ARCH_GET_GS:
	case ARCH_GET_FS:
		if (exiting(tcp)) {
			tprints_arg_next_name("addr");
			printnum_kptr(tcp, addr);
		}
		return 0;

	case ARCH_GET_CPUID: /* has no arguments */
		return RVAL_DECODED;

	case ARCH_GET_XCOMP_SUPP:
	case ARCH_GET_XCOMP_PERM:
	case ARCH_GET_XCOMP_GUEST_PERM:
		if (exiting(tcp)) {
			uint64_t val;

			tprints_arg_next_name("addr");
			if (umove_or_printaddr(tcp, addr, &val))
				return 0;

			/* XFEATURE_MASK_* macros are not publicly exposed */
			tprint_indirect_begin();
			printflags_ex(val, "XFEATURE_MASK_???",
				xlat_verbose(xlat_verbosity) == XLAT_STYLE_RAW
				      ? XLAT_STYLE_RAW : XLAT_STYLE_VERBOSE,
				x86_xfeatures, NULL);
			tprint_indirect_end();
		}

		return 0;

	case ARCH_REQ_XCOMP_PERM:
	case ARCH_REQ_XCOMP_GUEST_PERM:
		if (entering(tcp)) {
			/* XFEATURE_* enum is not publicly exposed */
			tprints_arg_next_name("addr");
			printxvals_ex(addr, "XFEATURE_???",
				xlat_verbose(xlat_verbosity) == XLAT_STYLE_RAW
				      ? XLAT_STYLE_RAW : XLAT_STYLE_VERBOSE,
				x86_xfeature_bits, NULL);
		} else {
			if (tcp->u_rval <= 0)
				return 0;

			tcp->rval_xlat = x86_xfeatures;

			return RVAL_FLAGS;
		}

		return 0;

	/* default handling: print arg2 in hexadecimal on entering */
	case ARCH_SET_GS:
	case ARCH_SET_FS:
	case ARCH_SET_CPUID:
	case ARCH_MAP_VDSO_X32:
	case ARCH_MAP_VDSO_32:
	case ARCH_MAP_VDSO_64:
	default:
		break;
	}

	tprints_arg_next_name("addr");
	PRINT_VAL_X(addr);

	return RVAL_DECODED;
}
#endif /* X86_64 || X32 || I386 */
