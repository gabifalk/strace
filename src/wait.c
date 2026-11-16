/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993-1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 2002-2004 Roland McGrath <roland@redhat.com>
 * Copyright (c) 2004 Ulrich Drepper <drepper@redhat.com>
 * Copyright (c) 2009-2013 Denys Vlasenko <dvlasenk@redhat.com>
 * Copyright (c) 2014-2015 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2014-2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"
#include "ptrace.h"

#include "wait.h"

#include "xlat/wait4_options.h"
#include "xlat/ptrace_events.h"

void
print_wait_status(int status)
{
	tprint_wait_status(status, ptrace_events);
}

static int
printwaitn(struct tcb *const tcp,
	   void (*const print_rusage)(struct tcb *, kernel_ulong_t))
{
	if (entering(tcp)) {
		/* pid */
		tprints_arg_name("pid");
		printpid_tgid_pgid(tcp, tcp->u_arg[0]);
	} else {
		int status;

		/* status */
		tprints_arg_next_name("status");
		if (tcp->u_rval == 0)
			printaddr(tcp->u_arg[1]);
		else if (!umove_or_printaddr(tcp, tcp->u_arg[1], &status))
			print_wait_status(status);

		/* options */
		tprints_arg_next_name("options");
		printflags(wait4_options, tcp->u_arg[2], "W???");
		if (print_rusage) {

			/* usage */
			tprints_arg_next_name("usage");
			if (tcp->u_rval > 0)
				print_rusage(tcp, tcp->u_arg[3]);
			else
				printaddr(tcp->u_arg[3]);
		}
	}
	return RVAL_TGID;
}

SYS_FUNC(waitpid)
{
	return printwaitn(tcp, NULL);
}

#if HAVE_ARCH_TIME32_SYSCALLS || HAVE_ARCH_OLD_TIME64_SYSCALLS
SYS_FUNC(wait4)
{
	return printwaitn(tcp, printrusage);
}
#endif

#ifdef ALPHA
SYS_FUNC(osf_wait4)
{
	return printwaitn(tcp, printrusage32);
}
#endif

#include "xlat/waitid_types.h"

SYS_FUNC(waitid)
{
	unsigned int idtype = (unsigned int) tcp->u_arg[0];
	int id = tcp->u_arg[1];

	if (entering(tcp)) {
		/* idtype */
		tprints_arg_name("idtype");
		printxval(waitid_types, idtype, "P_???");

		tprints_arg_next_name("id");
		switch (idtype) {
		case P_PID:
			printpid(tcp, id, PT_TGID);
			break;
		case P_PIDFD:
			printfd(tcp, id);
			break;
		case P_PGID:
			printpid(tcp, id, PT_PGID);
			break;
		default:
			PRINT_VAL_D(id);
			break;
		}
	} else {
		/* siginfo */
		tprints_arg_next_name("siginfo");
		printsiginfo_at(tcp, tcp->u_arg[2]);

		/* options */
		tprints_arg_next_name("options");
		printflags(wait4_options, tcp->u_arg[3], "W???");

		/* usage */
		tprints_arg_next_name("usage");
		printrusage(tcp, tcp->u_arg[4]);
	}
	return 0;
}
