/*
 * Copyright (c) 2016 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2016-2025 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"

#ifdef ALPHA

static long getxxid_rval2;

static void
print_getxxid_result(const char *const type, const char *const name,
		     const unsigned long long val)
{
	tprint_sysret_result_begin(type, val);
	tprints_sysret_result_field(name, true);
	PRINT_VAL_D_AS(type, getxxid_rval2);
	tprint_sysret_result_end();
}

static void
print_getxpid_result(struct tcb *const tcp, const unsigned long long val)
{
	print_getxxid_result("tgid", "ppid", val);
}

static void
print_getxuid_result(struct tcb *const tcp, const unsigned long long val)
{
	print_getxxid_result("uid", "euid", val);
}

static void
print_getxgid_result(struct tcb *const tcp, const unsigned long long val)
{
	print_getxxid_result("gid", "egid", val);
}

static int
decode_getxxid(struct tcb *tcp,
	       void (*const printer)(struct tcb *, unsigned long long))
{
	if (entering(tcp))
		return 0;

	long rval = getrval2(tcp);
	if (rval == -1)
		return 0;
	getxxid_rval2 = rval;
	tcp->rval_result_printer = printer;
	return RVAL_RESULT;
}

SYS_FUNC(getxpid)
{
	return decode_getxxid(tcp, print_getxpid_result);
}

SYS_FUNC(getxuid)
{
	return decode_getxxid(tcp, print_getxuid_result);
}

SYS_FUNC(getxgid)
{
	return decode_getxxid(tcp, print_getxgid_result);
}

SYS_FUNC(osf_statfs)
{
	/* pathname */
	tprints_arg_name("pathname");
	printpath(tcp, tcp->u_arg[0]);

	/* buf */
	tprints_arg_next_name("buf");
	printaddr(tcp->u_arg[1]);

	/* size */
	tprints_arg_next_name("size");
	PRINT_VAL_U(tcp->u_arg[2]);

	return RVAL_DECODED;
}

SYS_FUNC(osf_fstatfs)
{
	/* fd */
	tprints_arg_name("fd");
	printfd(tcp, tcp->u_arg[0]);

	/* buf */
	tprints_arg_next_name("buf");
	printaddr(tcp->u_arg[1]);

	/* size */
	tprints_arg_next_name("size");
	PRINT_VAL_U(tcp->u_arg[2]);

	return RVAL_DECODED;
}

#endif /* ALPHA */
