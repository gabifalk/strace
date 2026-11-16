/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 1999-2025 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"
#include "xstring.h"

static const char *fd_set_arg_name[] = {
	"readfds",
	"writefds",
	"exceptfds"
};

SYS_FUNC(close)
{
	tprints_arg_name("fd");
	printfd(tcp, tcp->u_arg[0]);

	return RVAL_DECODED;
}

/* The size of the buffer the result of select was formatted into. */
#define SELECT_RESULT_BUFSIZE 1024

struct select_result {
	kernel_ulong_t tv_ts_addr;
	print_obj_by_addr_fn print_tv_ts;
	bool left;
	struct {
		unsigned int nfds;
		int *fds;
	} sets[3];
	int fds[];
};

static const char *const select_result_names[] = { "in", "out", "except" };

static size_t
uint_len(unsigned int val)
{
	size_t len = 1;

	for (; val >= 10; val /= 10)
		++len;
	return len;
}

/*
 * Fetch the ready file descriptors and select the ones to print, applying
 * the limits of the buffer the result used to be formatted into.
 */
static struct select_result *
decode_select_result(struct tcb *const tcp, const kernel_ulong_t *const args,
		     const print_obj_by_addr_fn print_tv_ts,
		     const sprint_obj_by_addr_fn sprint_tv_ts)
{
	int nfds = (int) args[0];
	if (nfds < 0)
		nfds = 0;
	if (nfds > 1024*1024)
		nfds = 1024*1024;
	const int fdsize =
		(((nfds + 7) / 8) + current_wordsize-1) & -current_wordsize;
	int ready_fds = tcp->u_rval;
	struct select_result *const r =
		xzalloc(sizeof(*r) + sizeof(int) * ready_fds);
	fd_set *const fds = malloc(fdsize);
	size_t len = 0;
	const char *sep = "";
	int *next_fd = r->fds;

	r->tv_ts_addr = args[4];
	r->print_tv_ts = print_tv_ts;

	for (unsigned int i = 0; i < 3 && ready_fds > 0; ++i) {
		const kernel_ulong_t addr = args[i + 1];

		if (!addr || !fds || umoven(tcp, addr, fdsize, fds) < 0)
			continue;
		r->sets[i].fds = next_fd;
		for (int j = 0;; ++j) {
			j = next_set_bit(fds, j, nfds);
			if (j < 0)
				break;
			/* +2 chars needed at the end: ']',NUL */
			if (len < SELECT_RESULT_BUFSIZE - (sizeof(", except [")
							   + sizeof(int)*3 + 2)) {
				if (!r->sets[i].nfds) {
					len += strlen(sep)
					       + strlen(select_result_names[i])
					       + 2 + uint_len(j);
					sep = ", ";
				} else {
					len += 1 + uint_len(j);
				}
				*next_fd++ = j;
				++r->sets[i].nfds;
			}
			if (--ready_fds == 0)
				break;
		}
		if (r->sets[i].nfds)
			++len;
	}
	free(fds);

	if (args[4]) {
		const char *str = sprint_tv_ts(tcp, args[4]);
		if (len + sizeof("left ") + strlen(sep) + strlen(str)
		    < SELECT_RESULT_BUFSIZE)
			r->left = true;
	}

	return r;
}

static void
print_select_timeout(struct tcb *const tcp, const unsigned long long val)
{
	tprint_sysret_result_begin(val);
	tprints_sysret_result_note("Timeout");
	tprint_sysret_result_end();
}

static void
print_select_result(struct tcb *const tcp, const unsigned long long val)
{
	const struct select_result *const r = get_tcb_priv_data(tcp);
	bool printed = false;

	tprint_sysret_result_begin(val);
	for (unsigned int i = 0; i < 3; ++i) {
		if (r->sets[i].nfds) {
			tprints_sysret_result_field(select_result_names[i],
						    true);
			tprint_fd_set_begin();
			for (unsigned int k = 0; k < r->sets[i].nfds; ++k) {
				if (k)
					tprint_fd_set_next();
				tprint_object_begin();
				tprint_fd_num(r->sets[i].fds[k]);
				tprint_object_end();
			}
			tprint_fd_set_end();
			printed = true;
		}
	}
	if (r->left) {
		tprints_sysret_result_field("left", true);
		r->print_tv_ts(tcp, r->tv_ts_addr);
		printed = true;
	}
	if (!printed)
		tprints_sysret_result_note("");
	tprint_sysret_result_end();
}

static int
decode_select(struct tcb *const tcp, const kernel_ulong_t *const args,
	      const print_obj_by_addr_fn print_tv_ts,
	      const sprint_obj_by_addr_fn sprint_tv_ts)
{
	/* Kernel truncates args[0] to int, we do the same. */
	int nfds = (int) args[0];

	/* Kernel rejects negative nfds, so we don't parse it either. */
	if (nfds < 0)
		nfds = 0;

	/* Beware of select(2^31-1, NULL, NULL, NULL) and similar... */
	if (nfds > 1024*1024)
		nfds = 1024*1024;

	/*
	 * We had bugs a-la "while (j < args[0])" and "umoven(args[0])" below.
	 * Instead of args[0], use nfds for fd count, fdsize for array lengths.
	 */
	int fdsize = (((nfds + 7) / 8) + current_wordsize-1) & -current_wordsize;
	fd_set *fds = NULL;

	if (entering(tcp)) {
		tprints_arg_name("nfds");
		PRINT_VAL_D((int) args[0]);

		if (verbose(tcp) && fdsize > 0)
			fds = malloc(fdsize);
		for (unsigned int i = 0; i < 3; ++i) {
			kernel_ulong_t addr = args[i + 1];

			tprints_arg_next_name(fd_set_arg_name[i]);
			if (!fds) {
				printaddr(addr);
				continue;
			}
			if (umoven_or_printaddr(tcp, addr, fdsize, fds))
				continue;
			tprint_fd_set_begin();
			bool next = false;
			for (int j = 0;; ++j) {
				j = next_set_bit(fds, j, nfds);
				if (j < 0)
					break;
				if (next)
					tprint_fd_set_next();
				else
					next = true;
				tprint_object_begin();
				printfd(tcp, j);
				tprint_object_end();
			}
			tprint_fd_set_end();
		}
		free(fds);
		tprints_arg_next_name("timeout");
		print_tv_ts(tcp, args[4]);
	} else {
		if (syserror(tcp))
			return 0;

		if (tcp->u_rval == 0) {
			tcp->rval_result_printer = print_select_timeout;
			return RVAL_RESULT;
		}

		set_tcb_priv_data(tcp, decode_select_result(tcp, args,
							    print_tv_ts,
							    sprint_tv_ts),
				  free);
		tcp->rval_result_printer = print_select_result;
		return RVAL_RESULT;
	}
	return 0;
}

#if HAVE_ARCH_OLD_SELECT
SYS_FUNC(oldselect)
{
	kernel_ulong_t *args =
		fetch_indirect_syscall_args(tcp, tcp->u_arg[0], 5);

	if (args) {
		return decode_select(tcp, args, print_timeval, sprint_timeval);
	} else {
		if (entering(tcp)) {
			tprints_arg_name("arg");
			printaddr(tcp->u_arg[0]);
		}
		return RVAL_DECODED;
	}
}
#endif /* HAVE_ARCH_OLD_SELECT */

#ifdef ALPHA
SYS_FUNC(osf_select)
{
	return decode_select(tcp, tcp->u_arg, print_timeval32, sprint_timeval32);
}
#endif

SYS_FUNC(select)
{
	return decode_select(tcp, tcp->u_arg, print_timeval, sprint_timeval);
}

static int
do_pselect6(struct tcb *const tcp, const print_obj_by_addr_fn print_ts,
	    const sprint_obj_by_addr_fn sprint_ts)
{
	int rc = decode_select(tcp, tcp->u_arg, print_ts, sprint_ts);
	if (entering(tcp)) {
		tprints_arg_next_name("sig");
		print_kernel_sigset(tcp, tcp->u_arg[5]);
	}

	return rc;
}

#if HAVE_ARCH_TIME32_SYSCALLS
SYS_FUNC(pselect6_time32)
{
	return do_pselect6(tcp, print_timespec32, sprint_timespec32);
}
#endif

SYS_FUNC(pselect6_time64)
{
	return do_pselect6(tcp, print_timespec64, sprint_timespec64);
}
