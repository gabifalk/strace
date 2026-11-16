/*
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 1999-2025 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"
#include <poll.h>
#include "xstring.h"

#include "xlat/pollflags.h"

static bool
print_pollfd(struct tcb *tcp, void *elem_buf, size_t elem_size, void *data)
{
	const struct pollfd *fds = elem_buf;

	tprint_struct_begin();
	PRINT_FIELD_FD(*fds, fd, tcp);
	if (fds->fd >= 0) {
		tprint_struct_next();
		PRINT_FIELD_FLAGS(*fds, events, pollflags, "POLL???");
	}
	tprint_struct_end();

	return true;
}

static void
decode_poll_entering(struct tcb *tcp)
{
	const kernel_ulong_t addr = tcp->u_arg[0];
	const unsigned int nfds = tcp->u_arg[1];
	struct pollfd fds;

	/* fds */
	tprints_arg_name("fds");
	print_array(tcp, addr, nfds, &fds, sizeof(fds),
		    tfetch_mem, print_pollfd, 0);

	/* nfds */
	tprints_arg_next_name("nfds");
	PRINT_VAL_U(nfds);
}

/* The size of the buffer the result of poll was formatted into. */
#define POLL_RESULT_BUFSIZE 1024
/* The maximum number of entries that fit into that buffer. */
#define POLL_RESULT_MAX_ENTRIES \
	(POLL_RESULT_BUFSIZE / sizeof("{fd=0, revents=}"))

struct poll_result {
	print_obj_by_addr_fn print_ts;
	kernel_ulong_t pts;
	kernel_ulong_t fault_addr;
	enum { POLL_RESULT_END, POLL_RESULT_FAULT, POLL_RESULT_MORE } tail;
	bool left;
	bool left_more;
	unsigned int n;
	struct {
		int fd;
		unsigned short revents;
	} entries[POLL_RESULT_MAX_ENTRIES];
};

static void
print_poll_timeout(struct tcb *const tcp, const unsigned long long val)
{
	tprint_sysret_result_begin("poll_result", val);
	tprints_sysret_result_note("Timeout");
	tprint_sysret_result_end();
}

static void
print_poll_result(struct tcb *const tcp, const unsigned long long val)
{
	const struct poll_result *const r = get_tcb_priv_data(tcp);

	tprint_sysret_result_begin("poll_result", val);
	if (r->n || r->tail != POLL_RESULT_END) {
		tprints_sysret_result_field("fds", false);
		tprint_array_begin();
		for (unsigned int i = 0; i < r->n; ++i) {
			if (i)
				tprint_array_next();
			tprint_object_begin();
			tprint_struct_begin();
			tprints_field_name("fd");
			tprint_fd_num(r->entries[i].fd);
			tprint_struct_next();
			tprints_field_name("revents");
			printflags(pollflags, r->entries[i].revents, NULL);
			tprint_struct_end();
			tprint_object_end();
		}
		if (r->tail != POLL_RESULT_END) {
			if (r->n)
				tprint_array_next();
			if (r->tail == POLL_RESULT_FAULT) {
				tprint_object_begin();
				printaddr(r->fault_addr);
				tprint_object_end();
			} else {
				tprint_more_data_follows();
			}
		}
		tprint_array_end();
	}
	if (r->left) {
		tprints_sysret_result_field("left", true);
		r->print_ts(tcp, r->pts);
	} else if (r->left_more) {
		tprints_sysret_result_note("...");
	}
	tprint_sysret_result_end();
}

/*
 * Fetch the descriptors with returned events and select the ones to print,
 * applying the limits of the buffer the result used to be formatted into.
 */
static int
decode_poll_exiting(struct tcb *const tcp, const print_obj_by_addr_fn print_ts,
		    const sprint_obj_by_addr_fn sprint_ts,
		    const kernel_ulong_t pts)
{
	struct pollfd fds;
	const unsigned int nfds = tcp->u_arg[1];
	const unsigned long size = sizeof(fds) * nfds;
	const kernel_ulong_t start = tcp->u_arg[0];
	const kernel_ulong_t end = start + size;
	size_t len = 0;

	if (syserror(tcp))
		return 0;
	if (tcp->u_rval == 0) {
		tcp->rval_result_printer = print_poll_timeout;
		return RVAL_RESULT;
	}

	if (!verbose(tcp) || !start || !nfds ||
	    size / sizeof(fds) != nfds || end < start)
		return 0;

	struct poll_result *const r = xzalloc(sizeof(*r));
	r->print_ts = print_ts;
	r->pts = pts;

	for (kernel_ulong_t cur = start; cur < end; cur += sizeof(fds)) {
		if (umove(tcp, cur, &fds) < 0) {
			len += len ? 2 : 1;
			r->tail = POLL_RESULT_FAULT;
			r->fault_addr = cur;
			break;
		}
		if (!fds.revents)
			continue;
		len += len ? 2 : 1;
		/* printed starts with 0, hence printed + 1 */
		if (sequence_truncation_needed(tcp, r->n + 1)) {
			len += 3;
			r->tail = POLL_RESULT_MORE;
			break;
		}

		static const char fmt[] = "{fd=%d, revents=";
		char fdstr[sizeof(fmt) + sizeof(int) * 3];
		xsprintf(fdstr, fmt, fds.fd);

		const char *flagstr = sprintflags("", pollflags,
						  (unsigned short) fds.revents);

		if (len + strlen(fdstr) + strlen(flagstr) + 1 >=
		    POLL_RESULT_BUFSIZE
		    - (2 + 2 * sizeof(long) + sizeof(", ], ..."))
		    || r->n >= ARRAY_SIZE(r->entries)) {
			len += 3;
			r->tail = POLL_RESULT_MORE;
			break;
		}
		len += strlen(fdstr) + strlen(flagstr) + 1;
		r->entries[r->n].fd = fds.fd;
		r->entries[r->n].revents = fds.revents;
		++r->n;
	}

	if (len)
		++len;

	if (pts) {
		const char *str = sprint_ts(tcp, pts);

		if (len + sizeof(", left ") + strlen(str) < POLL_RESULT_BUFSIZE)
			r->left = true;
		else
			r->left_more = true;
	}

	if (!len && !r->left && !r->left_more) {
		free(r);
		return 0;
	}

	set_tcb_priv_data(tcp, r, free);
	tcp->rval_result_printer = print_poll_result;
	return RVAL_RESULT;
}

#if HAVE_ARCH_TIME32_SYSCALLS || HAVE_ARCH_OLD_TIME64_SYSCALLS
static int
do_poll(struct tcb *const tcp, const sprint_obj_by_addr_fn sprint_ts)
{
	if (entering(tcp)) {
		decode_poll_entering(tcp);

		/* timeout */
		tprints_arg_next_name("timeout");
		PRINT_VAL_D((int) tcp->u_arg[2]);

		return 0;
	} else {
		return decode_poll_exiting(tcp, NULL, sprint_ts, 0);
	}
}
#endif /* HAVE_ARCH_TIME32_SYSCALLS || HAVE_ARCH_OLD_TIME64_SYSCALLS */

#if HAVE_ARCH_TIME32_SYSCALLS
SYS_FUNC(poll_time32)
{
	return do_poll(tcp, sprint_timespec32);
}
#endif

#if HAVE_ARCH_OLD_TIME64_SYSCALLS
SYS_FUNC(poll_time64)
{
	return do_poll(tcp, sprint_timespec64);
}
#endif

static int
do_ppoll(struct tcb *const tcp, const print_obj_by_addr_fn print_ts,
	 const sprint_obj_by_addr_fn sprint_ts)
{
	if (entering(tcp)) {
		decode_poll_entering(tcp);

		/* tmo_p */
		tprints_arg_next_name("tmo_p");
		print_ts(tcp, tcp->u_arg[2]);

		/* sigmask */
		/* NB: kernel requires arg[4] == NSIG_BYTES */
		tprints_arg_next_name("sigmask");
		print_sigset_addr_len(tcp, tcp->u_arg[3], tcp->u_arg[4]);

		/* sigsetsize */
		tprints_arg_next_name("sigsetsize");
		PRINT_VAL_U(tcp->u_arg[4]);

		return 0;
	} else {
		return decode_poll_exiting(tcp, print_ts, sprint_ts,
					   tcp->u_arg[2]);
	}
}

#if HAVE_ARCH_TIME32_SYSCALLS
SYS_FUNC(ppoll_time32)
{
	return do_ppoll(tcp, print_timespec32, sprint_timespec32);
}
#endif

SYS_FUNC(ppoll_time64)
{
	return do_ppoll(tcp, print_timespec64, sprint_timespec64);
}
