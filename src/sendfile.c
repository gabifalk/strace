/*
 * Copyright (c) 2015 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2015-2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"

SYS_FUNC(sendfile64)
{
	uint64_t offset = 0;

	if (entering(tcp)) {
		/* out_fd */
		tprints_arg_name("out_fd");
		printfd(tcp, tcp->u_arg[0]);

		/* in_fd */
		tprints_arg_next_name("in_fd");
		printfd(tcp, tcp->u_arg[1]);

		/* offset */
		tprints_arg_next_name("offset");
		const int rc = umove_or_printaddr(tcp, tcp->u_arg[2], &offset);
		if (rc || !tcp->u_arg[3]) {
			if (rc == 0) {
				tprint_indirect_begin();
				PRINT_VAL_U(offset);
				tprint_indirect_end();
			}
			/* count */
			tprints_arg_next_name("count");
			PRINT_VAL_U(tcp->u_arg[3]);
			return RVAL_DECODED;
		}
		set_tcb_priv_data(tcp, xobjdup(&offset), free);
		tprint_indirect_inout_entering_u(offset);
	} else {
		const uint64_t *const entering_offset = get_tcb_priv_data(tcp);
		const bool fetched = !syserror(tcp) && tcp->u_rval &&
				     umove(tcp, tcp->u_arg[2], &offset) == 0;
		tprint_indirect_inout_exiting_u(2, "offset", fetched,
						*entering_offset, offset);

		/* count */
		tprints_arg_next_name("count");
		PRINT_VAL_U(tcp->u_arg[3]);
	}

	return 0;
}

SYS_FUNC(sendfile)
{
	uint64_t offset = 0;

	if (entering(tcp)) {
		/* out_fd */
		tprints_arg_name("out_fd");
		printfd(tcp, tcp->u_arg[0]);

		/* in_fd */
		tprints_arg_next_name("in_fd");
		printfd(tcp, tcp->u_arg[1]);

		/* offset */
		tprints_arg_next_name("offset");
		const int rc = umoven_to_uint64_or_printaddr(tcp,
							     tcp->u_arg[2],
							     current_wordsize,
							     &offset);
		if (rc || !tcp->u_arg[3]) {
			if (rc == 0) {
				tprint_indirect_begin();
				PRINT_VAL_U(offset);
				tprint_indirect_end();
			}
			/* count */
			tprints_arg_next_name("count");
			PRINT_VAL_U(tcp->u_arg[3]);
			return RVAL_DECODED;
		}
		set_tcb_priv_data(tcp, xobjdup(&offset), free);
		tprint_indirect_inout_entering_u(offset);
	} else {
		const uint64_t *const entering_offset = get_tcb_priv_data(tcp);
		const bool fetched = !syserror(tcp) && tcp->u_rval &&
				     tfetch_to_uint64(tcp, tcp->u_arg[2],
						      current_wordsize,
						      &offset);
		tprint_indirect_inout_exiting_u(2, "offset", fetched,
						*entering_offset, offset);

		/* count */
		tprints_arg_next_name("count");
		PRINT_VAL_U(tcp->u_arg[3]);
	}

	return 0;
}
