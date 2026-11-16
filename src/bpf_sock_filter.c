/*
 * Decoder of socket filter programs.
 *
 * Copyright (c) 2017 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2017-2022 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"

#include "bpf_filter.h"

#include <linux/filter.h>
#include "xlat/skf_ad.h"
#define XLAT_MACROS_ONLY
# include "xlat/skf_off.h"
#undef XLAT_MACROS_ONLY

static bool
print_sock_filter_k(const struct bpf_filter_block *const fp)
{
	if (BPF_CLASS(fp->code) == BPF_LD && BPF_MODE(fp->code) == BPF_ABS) {
		if (fp->k >= (unsigned int) SKF_AD_OFF) {
			tprint_skf_off_begin(fp->k, SKF_AD_OFF, "SKF_AD_OFF");
			printxval(skf_ad, fp->k - (unsigned int) SKF_AD_OFF,
				  "SKF_AD_???");
			tprint_skf_off_end();
			return true;
		} else if (fp->k >= (unsigned int) SKF_NET_OFF) {
			tprint_skf_off_begin(fp->k, SKF_NET_OFF, "SKF_NET_OFF");
			PRINT_VAL_U(fp->k - (unsigned int) SKF_NET_OFF);
			tprint_skf_off_end();
			return true;
		} else if (fp->k >= (unsigned int) SKF_LL_OFF) {
			tprint_skf_off_begin(fp->k, SKF_LL_OFF, "SKF_LL_OFF");
			PRINT_VAL_U(fp->k - (unsigned int) SKF_LL_OFF);
			tprint_skf_off_end();
			return true;
		}
	}

	return false;
}

void
print_sock_fprog(struct tcb *const tcp, const kernel_ulong_t addr,
		    const unsigned short len)
{
	print_bpf_fprog(tcp, addr, len, print_sock_filter_k);
}

void
decode_sock_fprog(struct tcb *const tcp, const kernel_ulong_t addr)
{
	decode_bpf_fprog(tcp, addr, print_sock_filter_k);
}
