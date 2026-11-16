/*
 * Copyright (c) 2010 Andreas Schwab <schwab@linux-m68k.org>
 * Copyright (c) 2012-2013 Denys Vlasenko <vda.linux@googlemail.com>
 * Copyright (c) 2014 Masatake YAMATO <yamato@redhat.com>
 * Copyright (c) 2010-2016 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2016-2025 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"
#include "msghdr.h"
#include "xstring.h"
#include <limits.h>

static bool
fetch_struct_mmsghdr_for_print(struct tcb *const tcp,
				  const kernel_ulong_t addr,
				  const unsigned int len, void *const mh)
{
	return (entering(tcp) || !syserror(tcp)) &&
	       fetch_struct_mmsghdr(tcp, addr, mh);
}

struct print_struct_mmsghdr_config {
	const int *p_user_msg_namelen;
	unsigned int msg_len_vlen;
	unsigned int count;
	bool use_msg_len;
};

static bool
print_struct_mmsghdr(struct tcb *tcp, void *elem_buf,
		     size_t elem_size, void *data)
{
	const struct mmsghdr *const mmsg = elem_buf;
	struct print_struct_mmsghdr_config *const c = data;

	if (!c->count) {
		tprint_more_data_follows();
		return false;
	}
	--c->count;

	tprint_struct_begin();
	if (c->p_user_msg_namelen)
		tprint_struct_mark_split();
	PRINT_FIELD_OBJ_TCB_PTR(*mmsg, msg_hdr, tcp, print_struct_msghdr,
			c->p_user_msg_namelen,
			c->use_msg_len ? mmsg->msg_len : (kernel_ulong_t) -1);
	if (c->msg_len_vlen) {
		tprint_struct_next();
		PRINT_FIELD_U(*mmsg, msg_len);
		--c->msg_len_vlen;
	}
	tprint_struct_end();

	if (c->p_user_msg_namelen)
		++c->p_user_msg_namelen;

	return true;
}

struct mmsgvec_data {
	struct saved_obj *timeout;
	print_obj_by_addr_fn print_ts;
	unsigned int count;
	int namelen[0];
};

static void
free_mmsgvec_data(void *ptr)
{
	struct mmsgvec_data *const data = ptr;

	free(data->timeout);
	free(data);
}

static void
save_mmsgvec_namelen(struct tcb *const tcp, kernel_ulong_t addr,
		     unsigned int len, struct saved_obj *const timeout,
		     const print_obj_by_addr_fn print_ts)
{
	if (len > IOV_MAX)
		len = IOV_MAX;

	const size_t data_size = offsetof(struct mmsgvec_data, namelen)
				 + sizeof(int) * len;
	struct mmsgvec_data *const data = xmalloc(data_size);
	data->timeout = timeout;
	data->print_ts = print_ts;

	unsigned int i, fetched;

	for (i = 0; i < len; ++i, addr += fetched) {
		struct mmsghdr mh;

		fetched = fetch_struct_mmsghdr(tcp, addr, &mh);
		if (!fetched)
			break;
		data->namelen[i] = mh.msg_hdr.msg_namelen;
	}
	data->count = i;

	set_tcb_priv_data(tcp, data, free_mmsgvec_data);

	/* msgvec */
	tprints_arg_mmsgvec_namelen_entering(1, "msgvec", data->namelen,
					     data->count);
}

static void
decode_mmsgvec(struct tcb *const tcp, const kernel_ulong_t addr,
	       const unsigned int vlen, const unsigned int msg_len_vlen,
	       const bool use_msg_len)
{
	struct mmsghdr mmsg;
	struct print_struct_mmsghdr_config c = {
		.msg_len_vlen = msg_len_vlen,
		.count = IOV_MAX,
		.use_msg_len = use_msg_len
	};
	const struct mmsgvec_data *const data = get_tcb_priv_data(tcp);

	if (data) {
		if (data->count < c.count)
			c.count = data->count;
		c.p_user_msg_namelen = data->namelen;
	}

	print_array(tcp, addr, vlen, &mmsg, sizeof_struct_mmsghdr(),
		    fetch_struct_mmsghdr_for_print,
		    print_struct_mmsghdr, &c);
}

void
dumpiov_in_mmsghdr(struct tcb *const tcp, kernel_ulong_t addr)
{
	unsigned int len = tcp->u_rval;
	unsigned int fetched;
	unsigned int i;

	for (i = 0; i < len; ++i, addr += fetched) {
		struct mmsghdr mmsg;
		fetched = fetch_struct_mmsghdr(tcp, addr, &mmsg);
		if (!fetched)
			break;
		if (!i)
			tprint_io_dump_mmsg_begin();
		tprint_io_dump_mmsg_vector_begin(mmsg.msg_hdr.msg_iovlen, i);
		dumpiov_upto(tcp, mmsg.msg_hdr.msg_iovlen,
			     ptr_to_kulong(mmsg.msg_hdr.msg_iov),
			     mmsg.msg_len);
		tprint_io_dump_mmsg_vector_end();
	}
	if (i)
		tprint_io_dump_mmsg_end();
}

SYS_FUNC(sendmmsg)
{
	if (entering(tcp)) {
		/* sockfd */
		tprints_arg_name("sockfd");
		printfd(tcp, tcp->u_arg[0]);

		if (!verbose(tcp)) {
			/* msgvec */
			tprints_arg_next_name("msgvec");
			printaddr(tcp->u_arg[1]);

			/* vlen */
			tprints_arg_next_name("vlen");
			PRINT_VAL_U((unsigned int) tcp->u_arg[2]);

			/* flags */
			tprints_arg_next_name("flags");
			printflags(msg_flags, tcp->u_arg[3], "MSG_???");
			return RVAL_DECODED;
		}
	} else {
		const unsigned int msg_len_vlen =
			syserror(tcp) ? 0 : tcp->u_rval;
		/* msgvec */
		tprints_arg_next_name("msgvec");
		temporarily_clear_syserror(tcp);
		decode_mmsgvec(tcp, tcp->u_arg[1], tcp->u_arg[2],
			       msg_len_vlen, false);
		restore_cleared_syserror(tcp);

		/* vlen */
		tprints_arg_next_name("vlen");
		PRINT_VAL_U((unsigned int) tcp->u_arg[2]);

		/* flags */
		tprints_arg_next_name("flags");
		printflags(msg_flags, tcp->u_arg[3], "MSG_???");
	}
	return 0;
}

static void
print_recvmmsg_timeout(struct tcb *const tcp, const unsigned long long val)
{
	tprint_sysret_result_begin("recvmmsg_result", val);
	tprints_sysret_result_note("Timeout");
	tprint_sysret_result_end();
}

static void
print_recvmmsg_result(struct tcb *const tcp, const unsigned long long val)
{
	const struct mmsgvec_data *const data = get_tcb_priv_data(tcp);

	tprint_sysret_result_begin("recvmmsg_result", val);
	tprints_sysret_result_field("left", true);
	data->print_ts(tcp, tcp->u_arg[4]);
	tprint_sysret_result_end();
}

static int
do_recvmmsg(struct tcb *const tcp, const print_obj_by_addr_fn print_ts,
	    const save_obj_by_addr_fn save_ts)
{
	if (entering(tcp)) {
		/* sockfd */
		tprints_arg_name("sockfd");
		printfd(tcp, tcp->u_arg[0]);

		if (verbose(tcp)) {
			save_mmsgvec_namelen(tcp, tcp->u_arg[1], tcp->u_arg[2],
					     save_ts(tcp, tcp->u_arg[4]),
					     print_ts);
		} else {
			/* msgvec */
			tprints_arg_next_name("msgvec");
			printaddr(tcp->u_arg[1]);

			/* vlen */
			tprints_arg_next_name("vlen");
			PRINT_VAL_U((unsigned int) tcp->u_arg[2]);

			/* flags */
			tprints_arg_next_name("flags");
			printflags(msg_flags, tcp->u_arg[3], "MSG_???");

			/* timeout */
			tprints_arg_next_name("timeout");
			print_ts(tcp, tcp->u_arg[4]);
		}
		return 0;
	} else {
		if (verbose(tcp)) {
			/* msgvec */
			tprints_arg_next_name("msgvec");
			decode_mmsgvec(tcp, tcp->u_arg[1], tcp->u_rval,
				       tcp->u_rval, true);

			/* vlen */
			tprints_arg_next_name("vlen");
			PRINT_VAL_U((unsigned int) tcp->u_arg[2]);

			/* flags */
			tprints_arg_next_name("flags");
			printflags(msg_flags, tcp->u_arg[3], "MSG_???");

			/* timeout on entrance */
			tprints_arg_next_name("timeout");
			print_saved_obj(((const struct mmsgvec_data *)
					 get_tcb_priv_data(tcp))->timeout);
		}
		if (syserror(tcp))
			return 0;
		if (tcp->u_rval == 0) {
			tcp->rval_result_printer = print_recvmmsg_timeout;
			return RVAL_RESULT;
		}
		if (!verbose(tcp) || !tcp->u_arg[4])
			return 0;
		/* timeout on exit */
		tcp->rval_result_printer = print_recvmmsg_result;
		return RVAL_RESULT;
	}
}

#if HAVE_ARCH_TIME32_SYSCALLS
SYS_FUNC(recvmmsg_time32)
{
	return do_recvmmsg(tcp, print_timespec32, save_timespec32);
}
#endif

SYS_FUNC(recvmmsg_time64)
{
	return do_recvmmsg(tcp, print_timespec64, save_timespec64);
}
