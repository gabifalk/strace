/*
 * Decoder of classic BPF programs.
 *
 * Copyright (c) 2015-2017 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2017-2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"

#include "bpf_filter.h"
#include "bpf_fprog.h"

#include <linux/filter.h>

#include "xlat/bpf_class.h"
#include "xlat/bpf_miscop.h"
#include "xlat/bpf_mode.h"
#include "xlat/bpf_op_alu.h"
#include "xlat/bpf_op_jmp.h"
#include "xlat/bpf_rval.h"
#include "xlat/bpf_size.h"
#include "xlat/bpf_src.h"

#include "xlat/ebpf_class.h"
#include "xlat/ebpf_mode.h"
#include "xlat/ebpf_op_alu.h"
#include "xlat/ebpf_op_jmp.h"
#include "xlat/ebpf_size.h"

static void
print_bpf_filter_code_part(const struct xlat *const xlat,
			   const struct xlat *const xlat2,
			   const uint16_t val, const bool first)
{
	if (!first)
		tprint_flags_groups_next();
	tprint_flags_group_begin(val);
	printxvals(val, "BPF_???", xlat, xlat2, NULL);
	tprint_flags_group_end();
}

static void
print_bpf_filter_code_rest(const uint16_t val)
{
	tprint_flags_groups_next();
	tprint_flags_group_begin(val);
	tprint_xval_dflt(val, "BPF_???");
	tprint_flags_group_end();
}

void
print_bpf_filter_code(const uint16_t code, bool extended)
{
	const struct xlat *mode = extended ? ebpf_mode : bpf_mode;
	uint16_t i = code & ~BPF_CLASS(code);

	tprint_flags_groups_begin(code);
	print_bpf_filter_code_part(extended ? ebpf_class : bpf_class, NULL,
				   BPF_CLASS(code), true);
	switch (BPF_CLASS(code)) {
	case BPF_ST:
	case BPF_STX:
		if (!extended) {
			if (i)
				print_bpf_filter_code_rest(i);
			break;
		}
		ATTRIBUTE_FALLTHROUGH; /* extended == true */

	case BPF_LD:
	case BPF_LDX:
		print_bpf_filter_code_part(bpf_size,
					   extended ? ebpf_size : NULL,
					   BPF_SIZE(code), false);
		print_bpf_filter_code_part(mode, NULL, BPF_MODE(code), false);
		break;

	case BPF_MISC: /* BPF_ALU64 in eBPF */
		if (!extended) {
			print_bpf_filter_code_part(bpf_miscop, NULL,
						   BPF_MISCOP(code), false);
			i &= ~BPF_MISCOP(code);
			if (i)
				print_bpf_filter_code_rest(i);
			break;
		}
		ATTRIBUTE_FALLTHROUGH; /* extended == true */

	case BPF_ALU:
		print_bpf_filter_code_part(bpf_src, NULL, BPF_SRC(code), false);
		print_bpf_filter_code_part(bpf_op_alu,
					   extended ? ebpf_op_alu : NULL,
					   BPF_OP(code), false);
		break;

	case BPF_JMP:
		print_bpf_filter_code_part(bpf_src, NULL, BPF_SRC(code), false);
		print_bpf_filter_code_part(bpf_op_jmp,
					   extended ? ebpf_op_jmp : NULL,
					   BPF_OP(code), false);
		break;

	case BPF_RET: /* Reserved in eBPF */
		if (!extended) {
			print_bpf_filter_code_part(bpf_rval, NULL,
						   BPF_RVAL(code), false);
			i &= ~BPF_RVAL(code);
		}

		if (i)
			print_bpf_filter_code_rest(i);

		break;
	}
	tprint_flags_groups_end();
}

static void
print_bpf_filter_stmt(const struct bpf_filter_block *const filter,
		      const print_bpf_filter_fn print_k)
{
	tprint_bpf_insn_begin(false);

	print_bpf_filter_code(filter->code, false);
	tprint_bpf_insn_k();

	if (!print_k || !print_k(filter))
		PRINT_VAL_X(filter->k);

	tprint_bpf_insn_end(false, 0, 0);
}

static void
print_bpf_filter_jump(const struct bpf_filter_block *const filter)
{
	tprint_bpf_insn_begin(true);

	print_bpf_filter_code(filter->code, false);
	tprint_bpf_insn_k();

	PRINT_VAL_X(filter->k);

	tprint_bpf_insn_end(true, filter->jt, filter->jf);
}

struct bpf_filter_block_data {
	const print_bpf_filter_fn fn;
	unsigned int count;
};

static bool
print_bpf_filter_block(struct tcb *const tcp, void *const elem_buf,
		       const size_t elem_size, void *const data)
{
	const struct bpf_filter_block *const filter = elem_buf;
	struct bpf_filter_block_data *const fbd = data;

	if (fbd->count++ >= BPF_MAXINSNS) {
		tprint_more_data_follows();
		return false;
	}

	if (filter->jt || filter->jf)
		print_bpf_filter_jump(filter);
	else
		print_bpf_filter_stmt(filter, fbd->fn);

	return true;
}

void
print_bpf_fprog(struct tcb *const tcp, const kernel_ulong_t addr,
		const unsigned short len, const print_bpf_filter_fn print_k)
{
	if (abbrev(tcp)) {
		printaddr(addr);
	} else {
		struct bpf_filter_block_data fbd = { .fn = print_k };
		struct bpf_filter_block filter;

		print_array(tcp, addr, len, &filter, sizeof(filter),
			    tfetch_mem, print_bpf_filter_block, &fbd);
	}
}

void
decode_bpf_fprog(struct tcb *const tcp, const kernel_ulong_t addr,
		 const print_bpf_filter_fn print_k)
{
	struct bpf_fprog fprog;

	if (fetch_bpf_fprog(tcp, addr, &fprog)) {
		tprint_struct_begin();
		PRINT_FIELD_U(fprog, len);
		tprint_struct_next();
		PRINT_FIELD_OBJ_TCB_VAL(fprog, filter, tcp,
					print_bpf_fprog, fprog.len, print_k);
		tprint_struct_end();
	}
}
