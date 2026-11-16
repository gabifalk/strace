/*
 * Helpers for emitting expected JSONL output in netlink tests.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef STRACE_TESTS_JSONL_NETLINK_H
# define STRACE_TESTS_JSONL_NETLINK_H

# include "tests.h"
# include "jsonl.h"
# include "scno.h"

# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <sys/socket.h>
# include "netlink.h"
# include <linux/rtnetlink.h>

/* Set by the test when it is run for the jsonl-merged mode. */
static bool jsonl_nl_merged;

/* The length, the result, and the errno of the last jsonl_nl_sendto. */
static unsigned int jsonl_nl_len;
static long jsonl_nl_rc;
static int jsonl_nl_errno;

/*
 * The payload of the netlink message or attribute sent last
 * by jsonl_nl_send_nlmsg or jsonl_nl_send_nlattr.
 */
static void *jsonl_nl_data;

/* Invoke sendto(fd, buf, len, MSG_DONTWAIT, NULL, 0) and save its result. */
static inline long
jsonl_nl_sendto(int fd, const void *buf, unsigned int len)
{
	jsonl_nl_len = len;
	jsonl_nl_rc = syscall(__NR_sendto, fd, buf, len, MSG_DONTWAIT, NULL, 0);
	jsonl_nl_errno = errno;
	return jsonl_nl_rc;
}

/* The event of the last jsonl_nl_sendto up to the fields of the buf value. */
static inline void
jsonl_nl_sendto_open(int fd)
{
	const char *side = jsonl_nl_merged ? "entry" : NULL;

	if (jsonl_nl_merged)
		jsonl_syscall_merged_open("sendto", __NR_sendto);
	else
		jsonl_syscall_open("sendto", __NR_sendto, true);
	jsonl_arg_open(0, "sockfd", side);
	jsonl_fd(fd, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "buf", side);
}

/* The rest of the event after the fields of the buf value. */
static inline void
jsonl_nl_sendto_close(void)
{
	static const struct strval32 msg_flags[] = {
		{ ARG_STR(MSG_DONTWAIT) }
	};
	const char *side = jsonl_nl_merged ? "entry" : NULL;

	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(2, "len", side);
	jsonl_unsigned(jsonl_nl_len);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(3, "flags", side);
	jsonl_flags(MSG_DONTWAIT, msg_flags, ARRAY_SIZE(msg_flags), NULL);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(4, "dest_addr", side);
	jsonl_addr(NULL);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(5, "addrlen", side);
	jsonl_decimal(0);
	jsonl_arg_close(side);
	if (!jsonl_nl_merged) {
		printf("]}\n");
		jsonl_syscall_open("sendto", __NR_sendto, false);
	}
	printf("], ");
	errno = jsonl_nl_errno;
	jsonl_return("unsigned", jsonl_nl_rc);
	printf("}\n");
}

/*
 * A struct nlmsghdr value up to the fields of its nlmsg_type value,
 * for a message type that is not a plain const.
 */
static inline void
jsonl_nlmsghdr_type_open(unsigned int len)
{
	jsonl_struct_open();
	jsonl_field_open("nlmsg_len", true);
	jsonl_unsigned(len);
	jsonl_field_close();
	jsonl_field_open("nlmsg_type", false);
}

/* The rest of a struct nlmsghdr value after the fields of nlmsg_type. */
static inline void
jsonl_nlmsghdr_type_close(unsigned int flags, const struct strval32 *known,
			  size_t n, unsigned int seq, int pid)
{
	jsonl_field_close();
	jsonl_field_open("nlmsg_flags", false);
	jsonl_flags(flags, known, n, NULL);
	jsonl_field_close();
	jsonl_field_open("nlmsg_seq", false);
	jsonl_unsigned(seq);
	jsonl_field_close();
	jsonl_field_open("nlmsg_pid", false);
	jsonl_pid("tgid", pid);
	jsonl_field_close();
	jsonl_struct_close();
}

/*
 * The fields of a struct nlmsghdr value with zero nlmsg_seq and nlmsg_pid;
 * nlmsg_type is a const, the names of nlmsg_flags are taken from known.
 */
static inline void
jsonl_nlmsghdr_vals(unsigned int len, unsigned int type, const char *type_sym,
		    const char *type_pattern, unsigned int flags,
		    const struct strval32 *known, size_t n)
{
	jsonl_nlmsghdr_type_open(len);
	jsonl_const(type, true, type_sym, type_pattern);
	jsonl_nlmsghdr_type_close(flags, known, n, 0, 0);
}

/* Likewise, for the header at nlh, which does not have to be aligned. */
static inline void
jsonl_nlmsghdr(const void *nlh, const char *type_sym, const char *type_pattern,
	       const struct strval32 *known, size_t n)
{
	struct nlmsghdr h;

	memcpy(&h, nlh, sizeof(h));
	jsonl_nlmsghdr_type_open(h.nlmsg_len);
	jsonl_const(h.nlmsg_type, true, type_sym, type_pattern);
	jsonl_nlmsghdr_type_close(h.nlmsg_flags, known, n,
				  h.nlmsg_seq, (int) h.nlmsg_pid);
}

/*
 * A netlink message with a payload: an array value up to the fields
 * of its second element, the first element being the header at nlh.
 */
static inline void
jsonl_nlmsg_open(const void *nlh, const char *type_sym,
		 const char *type_pattern, const struct strval32 *known,
		 size_t n)
{
	jsonl_array_open();
	printf("{");
	jsonl_nlmsghdr(nlh, type_sym, type_pattern, known, n);
	printf("}, {");
}

/* The separator between the fields of two elements of a message. */
static inline void
jsonl_nlmsg_next(void)
{
	printf("}, {");
}

/* Close the last element and the array opened by jsonl_nlmsg_open. */
static inline void
jsonl_nlmsg_close(void)
{
	printf("}");
	jsonl_array_close();
}

/*
 * The fields of a struct nlattr value; nla_type is a const, or, when
 * it has NLA_F_NESTED or NLA_F_NET_BYTEORDER set, a flags value that
 * ends with that const.
 */
static inline void
jsonl_nlattr_hdr(unsigned int nla_len, unsigned int nla_type,
		 const char *sym, const char *pattern)
{
	const unsigned int type = nla_type & NLA_TYPE_MASK;

	jsonl_struct_open();
	jsonl_field_open("nla_len", true);
	jsonl_unsigned(nla_len);
	jsonl_field_close();
	jsonl_field_open("nla_type", false);
	if (nla_type & ~NLA_TYPE_MASK) {
		bool first = true;

		jsonl_flags_open(nla_type);
		if (nla_type & NLA_F_NESTED) {
			jsonl_flags_const(NLA_F_NESTED, "NLA_F_NESTED", first);
			first = false;
		}
		if (nla_type & NLA_F_NET_BYTEORDER) {
			jsonl_flags_const(NLA_F_NET_BYTEORDER,
					  "NLA_F_NET_BYTEORDER", first);
			first = false;
		}
		printf("%s{", first ? "" : ", ");
		jsonl_const(type, true, sym, pattern);
		printf("}");
		jsonl_flags_close();
	} else {
		jsonl_const(type, true, sym, pattern);
	}
	jsonl_field_close();
	jsonl_struct_close();
}

/*
 * A netlink attribute with a payload: an array value up to the fields
 * of its second element, the first element being the struct nlattr.
 */
static inline void
jsonl_nlattr_open(unsigned int nla_len, unsigned int nla_type,
		  const char *sym, const char *pattern)
{
	jsonl_array_open();
	printf("{");
	jsonl_nlattr_hdr(nla_len, nla_type, sym, pattern);
	printf("}, {");
}

/* Close the last element and the array opened by jsonl_nlattr_open. */
static inline void
jsonl_nlattr_close(void)
{
	printf("}");
	jsonl_array_close();
}

/*
 * For a print_msg_ function of the JSONL_NESTED_NLATTR macros: a message
 * element with the enclosing attribute, up to the end of its struct nlattr.
 */
static inline void
jsonl_nlattr_nest_open(unsigned int nla_len, unsigned int nla_type,
		       const char *sym, const char *pattern)
{
	printf(", {");
	jsonl_array_open();
	printf("{");
	jsonl_nlattr_hdr(nla_len, nla_type, sym, pattern);
	printf("}");
}

/* Close depth enclosing attributes opened by jsonl_nlattr_nest_open. */
static inline void
jsonl_nlattr_nest_close(unsigned int depth)
{
	while (depth--) {
		jsonl_array_close();
		printf("}");
	}
}

/* The fallback of the JSONL_*_OBJECT macros: a bytes value. */
static inline void
jsonl_nl_bytes(const void *p, size_t len)
{
	jsonl_bytes_value(p, len, false);
}

/*
 * Send a message with a payload of data_len bytes whose first slen bytes,
 * copied from src, end at nlh0, the end of the readable memory.
 */
static inline void
jsonl_nl_send_nlmsg(int fd, void *nlh0, unsigned int type, unsigned int flags,
		    unsigned int data_len, const void *src, unsigned int slen)
{
	struct nlmsghdr *const nlh = nlh0 - slen;

	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = NLMSG_HDRLEN + data_len,
		.nlmsg_type = type,
		.nlmsg_flags = flags
	);
	jsonl_nl_data = NLMSG_DATA(nlh);
	memcpy(jsonl_nl_data, src, slen);

	jsonl_nl_sendto(fd, nlh, NLMSG_HDRLEN + data_len);
}

/*
 * The JSONL counterpart of TEST_NETLINK_ from test_netlink.h: send a message
 * and print its event.  nlmsg_type is a const described by type_sym_ and
 * type_pattern_, the names of flags_ are taken from the nknown_ entries of
 * known_.  The variable arguments print the fields of the payload value,
 * using jsonl_nlmsg_next between the elements if there are several; the
 * payload is at jsonl_nl_data.
 */
# define JSONL_NETLINK_(fd_, nlh0_, type_, type_sym_, type_pattern_,	\
			flags_, known_, nknown_,			\
			data_len_, src_, slen_, ...)			\
	do {								\
		jsonl_nl_send_nlmsg((fd_), (nlh0_), (type_), (flags_),	\
				    (data_len_), (src_), (slen_));	\
		jsonl_nl_sendto_open(fd_);				\
		jsonl_array_open();					\
		printf("{");						\
		jsonl_nlmsghdr_vals(jsonl_nl_len, (type_), (type_sym_),	\
				    (type_pattern_), (flags_),		\
				    (known_), (nknown_));		\
		printf("}, {");						\
		{ __VA_ARGS__; }					\
		printf("}");						\
		jsonl_array_close();					\
		jsonl_nl_sendto_close();				\
	} while (0)

/* Likewise, for a known message type and a single known flag. */
# define JSONL_NETLINK(fd_, nlh0_, type_, flags_,			\
		       data_len_, src_, slen_, ...)			\
	JSONL_NETLINK_((fd_), (nlh0_), (type_), #type_, NULL,		\
		       (flags_),					\
		       (&(const struct strval32) { (flags_), #flags_ }), \
		       1,						\
		       (data_len_), (src_), (slen_), __VA_ARGS__)

/*
 * The JSONL counterpart of TEST_NETLINK_OBJECT_EX_: a message too short
 * for obj_, printed by fallback_func(ptr, len), a message whose object is
 * not entirely readable, printed as an addr value, and a message with obj_,
 * printed by the variable arguments.
 */
# define JSONL_NETLINK_OBJECT_EX_(fd_, nlh0_, type_, type_sym_,		\
				  type_pattern_, flags_, known_, nknown_, \
				  obj_, fallback_func, ...)		\
	do {								\
		char pattern[DEFAULT_STRLEN];				\
		fill_memory_ex(pattern, sizeof(pattern),		\
			       'a', 'z' - 'a' + 1);			\
		const unsigned int plen =				\
			MIN(sizeof(obj_) - 1, DEFAULT_STRLEN);		\
		/* len < sizeof(obj_) */				\
		JSONL_NETLINK_((fd_), (nlh0_), (type_), (type_sym_),	\
			       (type_pattern_), (flags_),		\
			       (known_), (nknown_),			\
			       plen, pattern, plen,			\
			       (fallback_func)(pattern, plen));		\
		/* short read of sizeof(obj_) */			\
		JSONL_NETLINK_((fd_), (nlh0_), (type_), (type_sym_),	\
			       (type_pattern_), (flags_),		\
			       (known_), (nknown_),			\
			       sizeof(obj_), pattern, plen,		\
			       jsonl_addr(jsonl_nl_data));		\
		/* sizeof(obj_) */					\
		JSONL_NETLINK_((fd_), (nlh0_), (type_), (type_sym_),	\
			       (type_pattern_), (flags_),		\
			       (known_), (nknown_),			\
			       sizeof(obj_), &(obj_), sizeof(obj_),	\
			       __VA_ARGS__);				\
	} while (0)

/* Likewise, for a known message type, a single known flag, and bytes. */
# define JSONL_NETLINK_OBJECT(fd_, nlh0_, type_, flags_, obj_, ...)	\
	JSONL_NETLINK_OBJECT_EX_((fd_), (nlh0_), (type_), #type_, NULL,	\
				 (flags_),				\
				 (&(const struct strval32) {		\
					(flags_), #flags_ }), 1,	\
				 (obj_), jsonl_nl_bytes, __VA_ARGS__)

/*
 * Send a message that consists of a header, hdrlen bytes initialized by
 * init_msg, and an attribute at the nesting level depth with a payload of
 * nla_data_len bytes whose first slen bytes, copied from src, end at nlh0,
 * the end of the readable memory.
 */
static inline void
jsonl_nl_send_nlattr(int fd, void *nlh0, unsigned int hdrlen,
		     void (*init_msg)(struct nlmsghdr *, unsigned int),
		     unsigned int nla_type, unsigned int nla_data_len,
		     const void *src, unsigned int slen, unsigned int depth)
{
	hdrlen += NLA_HDRLEN * depth;

	struct nlmsghdr *const nlh =
		nlh0 - NLA_HDRLEN * depth - (NLA_HDRLEN + slen);
	struct nlattr *const nla = NLMSG_ATTR(nlh, hdrlen);
	const unsigned int msg_len =
		NLMSG_SPACE(hdrlen) + NLA_HDRLEN + nla_data_len;

	init_msg(nlh, msg_len);
	SET_STRUCT(struct nlattr, nla,
		.nla_len = NLA_HDRLEN + nla_data_len,
		.nla_type = nla_type
	);
	jsonl_nl_data = RTA_DATA(nla);
	memcpy(jsonl_nl_data, src, slen);

	jsonl_nl_sendto(fd, nlh, msg_len);
}

/*
 * The JSONL counterpart of TEST_NESTED_NLATTR_ from test_nlattr.h: send
 * a message with an attribute and print its event.  As there, init_msg_
 * initializes the message including the depth_ enclosing attributes, and
 * print_msg_(msg_len) prints what precedes the attribute: the elements
 * with the netlink header and the fixed part of the message, followed by
 * a jsonl_nlattr_nest_open for every enclosing attribute.  nla_type is
 * a const described by sym_ and pattern_.  The variable arguments print
 * the fields of the attribute payload value, which is at jsonl_nl_data.
 */
# define JSONL_NESTED_NLATTR_(fd_, nlh0_, hdrlen_,			\
			      init_msg_, print_msg_,			\
			      nla_type_, sym_, pattern_,		\
			      nla_data_len_, src_, slen_, depth_, ...)	\
	do {								\
		jsonl_nl_send_nlattr((fd_), (nlh0_), (hdrlen_),		\
				     (init_msg_), (nla_type_),		\
				     (nla_data_len_), (src_), (slen_),	\
				     (depth_));				\
		jsonl_nl_sendto_open(fd_);				\
		jsonl_array_open();					\
		(print_msg_)(jsonl_nl_len);				\
		printf(", {");						\
		jsonl_nlattr_open(NLA_HDRLEN + (nla_data_len_),		\
				  (nla_type_), (sym_), (pattern_));	\
		{ __VA_ARGS__; }					\
		jsonl_nlattr_close();					\
		printf("}");						\
		jsonl_nlattr_nest_close(depth_);			\
		jsonl_array_close();					\
		jsonl_nl_sendto_close();				\
	} while (0)

/* The JSONL counterpart of TEST_NLATTR_. */
# define JSONL_NLATTR_(fd_, nlh0_, hdrlen_, init_msg_, print_msg_,	\
		       nla_type_, sym_, pattern_,			\
		       nla_data_len_, src_, slen_, ...)			\
	JSONL_NESTED_NLATTR_((fd_), (nlh0_), (hdrlen_),			\
			     (init_msg_), (print_msg_),			\
			     (nla_type_), (sym_), (pattern_),		\
			     (nla_data_len_), (src_), (slen_), 0,	\
			     __VA_ARGS__)

/* The JSONL counterpart of TEST_NLATTR: a known attribute type. */
# define JSONL_NLATTR(fd_, nlh0_, hdrlen_, init_msg_, print_msg_,	\
		      nla_type_, nla_data_len_, src_, slen_, ...)	\
	JSONL_NLATTR_((fd_), (nlh0_), (hdrlen_),			\
		      (init_msg_), (print_msg_),			\
		      (nla_type_), #nla_type_, NULL,			\
		      (nla_data_len_), (src_), (slen_), __VA_ARGS__)

/*
 * The JSONL counterpart of TEST_NESTED_NLATTR_OBJECT_EX_MINSZ_ and
 * TEST_NLATTR_OBJPTR_EX_: an attribute too short for the object, printed
 * by fallback_func(ptr, len), an attribute whose first minsz_ bytes are not
 * entirely readable, printed as an addr value, and an attribute with the
 * object of objsz_ bytes at objptr_, printed by the variable arguments.
 */
# define JSONL_NESTED_NLATTR_OBJPTR_EX_(fd_, nlh0_, hdrlen_,		\
					init_msg_, print_msg_,		\
					nla_type_, sym_, pattern_,	\
					pattern_buf_, objptr_, objsz_,	\
					minsz_, fallback_func, depth_, ...) \
	do {								\
		const unsigned int plen =				\
			MIN((minsz_) - 1, DEFAULT_STRLEN);		\
		/* len < objsz_ */					\
		if (plen > 0)						\
			JSONL_NESTED_NLATTR_((fd_), (nlh0_), (hdrlen_),	\
				(init_msg_), (print_msg_),		\
				(nla_type_), (sym_), (pattern_),	\
				plen, (pattern_buf_), plen, (depth_),	\
				(fallback_func)((pattern_buf_), plen));	\
		/* short read of objsz_ */				\
		JSONL_NESTED_NLATTR_((fd_), (nlh0_), (hdrlen_),		\
			(init_msg_), (print_msg_),			\
			(nla_type_), (sym_), (pattern_),		\
			(objsz_), (pattern_buf_), (minsz_) - 1,		\
			(depth_),					\
			jsonl_addr(jsonl_nl_data));			\
		/* objsz_ */						\
		JSONL_NESTED_NLATTR_((fd_), (nlh0_), (hdrlen_),		\
			(init_msg_), (print_msg_),			\
			(nla_type_), (sym_), (pattern_),		\
			(objsz_), (objptr_), (objsz_), (depth_),	\
			__VA_ARGS__);					\
	} while (0)

/* The JSONL counterpart of TEST_NESTED_NLATTR_OBJECT_EX_. */
# define JSONL_NESTED_NLATTR_OBJECT_EX_(fd_, nlh0_, hdrlen_,		\
					init_msg_, print_msg_,		\
					nla_type_, sym_, pattern_,	\
					pattern_buf_, obj_,		\
					fallback_func, depth_, ...)	\
	JSONL_NESTED_NLATTR_OBJPTR_EX_((fd_), (nlh0_), (hdrlen_),	\
				       (init_msg_), (print_msg_),	\
				       (nla_type_), (sym_), (pattern_),	\
				       (pattern_buf_), &(obj_),		\
				       sizeof(obj_), sizeof(obj_),	\
				       (fallback_func), (depth_),	\
				       __VA_ARGS__)

/* The JSONL counterpart of TEST_NESTED_NLATTR_OBJECT_EX. */
# define JSONL_NESTED_NLATTR_OBJECT_EX(fd_, nlh0_, hdrlen_,		\
				       init_msg_, print_msg_,		\
				       nla_type_, pattern_buf_, obj_,	\
				       depth_, ...)			\
	JSONL_NESTED_NLATTR_OBJECT_EX_((fd_), (nlh0_), (hdrlen_),	\
				       (init_msg_), (print_msg_),	\
				       (nla_type_), #nla_type_, NULL,	\
				       (pattern_buf_), (obj_),		\
				       jsonl_nl_bytes, (depth_),	\
				       __VA_ARGS__)

/* The JSONL counterpart of TEST_NESTED_NLATTR_OBJECT. */
# define JSONL_NESTED_NLATTR_OBJECT(fd_, nlh0_, hdrlen_,		\
				    init_msg_, print_msg_,		\
				    nla_type_, pattern_buf_, obj_, ...)	\
	JSONL_NESTED_NLATTR_OBJECT_EX((fd_), (nlh0_), (hdrlen_),	\
				      (init_msg_), (print_msg_),	\
				      nla_type_, (pattern_buf_), (obj_), \
				      1, __VA_ARGS__)

/* The JSONL counterpart of TEST_NLATTR_OBJECT_EX_. */
# define JSONL_NLATTR_OBJECT_EX_(fd_, nlh0_, hdrlen_,			\
				 init_msg_, print_msg_,			\
				 nla_type_, sym_, pattern_,		\
				 pattern_buf_, obj_, minsz_,		\
				 fallback_func, ...)			\
	JSONL_NESTED_NLATTR_OBJPTR_EX_((fd_), (nlh0_), (hdrlen_),	\
				       (init_msg_), (print_msg_),	\
				       (nla_type_), (sym_), (pattern_),	\
				       (pattern_buf_), &(obj_),		\
				       sizeof(obj_), (minsz_),		\
				       (fallback_func), 0, __VA_ARGS__)

/* The JSONL counterpart of TEST_NLATTR_OBJECT_EX. */
# define JSONL_NLATTR_OBJECT_EX(fd_, nlh0_, hdrlen_,			\
				init_msg_, print_msg_,			\
				nla_type_, pattern_buf_, obj_, minsz_,	\
				fallback_func, ...)			\
	JSONL_NLATTR_OBJECT_EX_((fd_), (nlh0_), (hdrlen_),		\
				(init_msg_), (print_msg_),		\
				(nla_type_), #nla_type_, NULL,		\
				(pattern_buf_), (obj_), (minsz_),	\
				(fallback_func), __VA_ARGS__)

/* The JSONL counterpart of TEST_NLATTR_OBJECT_. */
# define JSONL_NLATTR_OBJECT_(fd_, nlh0_, hdrlen_,			\
			      init_msg_, print_msg_,			\
			      nla_type_, sym_, pattern_,		\
			      pattern_buf_, obj_, ...)			\
	JSONL_NLATTR_OBJECT_EX_((fd_), (nlh0_), (hdrlen_),		\
				(init_msg_), (print_msg_),		\
				(nla_type_), (sym_), (pattern_),	\
				(pattern_buf_), (obj_), sizeof(obj_),	\
				jsonl_nl_bytes, __VA_ARGS__)

/* The JSONL counterpart of TEST_NLATTR_OBJECT: a known attribute type. */
# define JSONL_NLATTR_OBJECT(fd_, nlh0_, hdrlen_,			\
			     init_msg_, print_msg_,			\
			     nla_type_, pattern_buf_, obj_, ...)	\
	JSONL_NLATTR_OBJECT_((fd_), (nlh0_), (hdrlen_),			\
			     (init_msg_), (print_msg_),			\
			     (nla_type_), #nla_type_, NULL,		\
			     (pattern_buf_), (obj_), __VA_ARGS__)

/* The JSONL counterpart of TEST_NLATTR_OBJECT_MINSZ. */
# define JSONL_NLATTR_OBJECT_MINSZ(fd_, nlh0_, hdrlen_,			\
				   init_msg_, print_msg_,		\
				   nla_type_, pattern_buf_, obj_,	\
				   minsz_, ...)				\
	JSONL_NLATTR_OBJECT_EX_((fd_), (nlh0_), (hdrlen_),		\
				(init_msg_), (print_msg_),		\
				(nla_type_), #nla_type_, NULL,		\
				(pattern_buf_), (obj_), (minsz_),	\
				jsonl_nl_bytes, __VA_ARGS__)

/* An array value with the first n_ elements of obj_. */
# define JSONL_NLATTR_ARRAY_ELEMS_(obj_, n_, print_elem_)		\
	do {								\
		jsonl_array_open();					\
		for (size_t elem_i_ = 0; elem_i_ < (n_); ++elem_i_) {	\
			printf("%s{", elem_i_ ? ", " : "");		\
			(print_elem_)(&(obj_)[elem_i_], elem_i_);	\
			printf("}");					\
		}							\
	} while (0)

/*
 * The JSONL counterpart of TEST_NESTED_NLATTR_ARRAY_EX_: an attribute too
 * short for an element of the array obj_, printed as bytes, an attribute
 * with all elements but the last, an attribute whose last element is not
 * entirely readable, and an attribute with the whole array;
 * print_elem_(&obj_[i], i) prints the fields of an element value.
 */
# define JSONL_NESTED_NLATTR_ARRAY_EX_(fd_, nlh0_, hdrlen_,		\
				       init_msg_, print_msg_,		\
				       nla_type_, sym_, pattern_,	\
				       pattern_buf_, obj_, depth_,	\
				       print_elem_)			\
	do {								\
		const unsigned int plen =				\
			MIN(sizeof((obj_)[0]) - 1, DEFAULT_STRLEN);	\
		/* len < sizeof((obj_)[0]) */				\
		JSONL_NESTED_NLATTR_((fd_), (nlh0_), (hdrlen_),		\
			(init_msg_), (print_msg_),			\
			(nla_type_), (sym_), (pattern_),		\
			plen, (pattern_buf_), plen, (depth_),		\
			jsonl_nl_bytes((pattern_buf_), plen));		\
		/* sizeof((obj_)[0]) < len < sizeof(obj_) */		\
		JSONL_NESTED_NLATTR_((fd_), (nlh0_), (hdrlen_),		\
			(init_msg_), (print_msg_),			\
			(nla_type_), (sym_), (pattern_),		\
			sizeof(obj_) - 1,				\
			&(obj_), sizeof(obj_) - 1, (depth_),		\
			JSONL_NLATTR_ARRAY_ELEMS_((obj_),		\
						  ARRAY_SIZE(obj_) - 1,	\
						  (print_elem_));	\
			jsonl_array_close());				\
		/* short read of sizeof(obj_) */			\
		JSONL_NESTED_NLATTR_((fd_), (nlh0_), (hdrlen_),		\
			(init_msg_), (print_msg_),			\
			(nla_type_), (sym_), (pattern_),		\
			sizeof(obj_),					\
			&(obj_), sizeof(obj_) - 1, (depth_),		\
			JSONL_NLATTR_ARRAY_ELEMS_((obj_),		\
						  ARRAY_SIZE(obj_) - 1,	\
						  (print_elem_));	\
			jsonl_array_fetch_failed(jsonl_nl_data		\
						 + sizeof(obj_)		\
						 - sizeof((obj_)[0])));	\
		/* sizeof(obj_) */					\
		JSONL_NESTED_NLATTR_((fd_), (nlh0_), (hdrlen_),		\
			(init_msg_), (print_msg_),			\
			(nla_type_), (sym_), (pattern_),		\
			sizeof(obj_), &(obj_), sizeof(obj_), (depth_),	\
			JSONL_NLATTR_ARRAY_ELEMS_((obj_),		\
						  ARRAY_SIZE(obj_),	\
						  (print_elem_));	\
			jsonl_array_close());				\
	} while (0)

/* The JSONL counterpart of TEST_NESTED_NLATTR_ARRAY_EX. */
# define JSONL_NESTED_NLATTR_ARRAY_EX(fd_, nlh0_, hdrlen_,		\
				      init_msg_, print_msg_,		\
				      nla_type_, pattern_buf_, obj_,	\
				      depth_, print_elem_)		\
	JSONL_NESTED_NLATTR_ARRAY_EX_((fd_), (nlh0_), (hdrlen_),	\
				      (init_msg_), (print_msg_),	\
				      (nla_type_), #nla_type_, NULL,	\
				      (pattern_buf_), obj_, (depth_),	\
				      (print_elem_))

/* The JSONL counterpart of TEST_NESTED_NLATTR_ARRAY. */
# define JSONL_NESTED_NLATTR_ARRAY(fd_, nlh0_, hdrlen_,			\
				   init_msg_, print_msg_,		\
				   nla_type_, pattern_buf_, obj_,	\
				   print_elem_)				\
	JSONL_NESTED_NLATTR_ARRAY_EX((fd_), (nlh0_), (hdrlen_),		\
				     (init_msg_), (print_msg_),		\
				     nla_type_, (pattern_buf_), obj_, 1, \
				     (print_elem_))

/* The JSONL counterpart of TEST_NLATTR_ARRAY_. */
# define JSONL_NLATTR_ARRAY_(fd_, nlh0_, hdrlen_,			\
			     init_msg_, print_msg_,			\
			     nla_type_, sym_, pattern_,			\
			     pattern_buf_, obj_, print_elem_)		\
	JSONL_NESTED_NLATTR_ARRAY_EX_((fd_), (nlh0_), (hdrlen_),	\
				      (init_msg_), (print_msg_),	\
				      (nla_type_), (sym_), (pattern_),	\
				      (pattern_buf_), obj_, 0,		\
				      (print_elem_))

/* The JSONL counterpart of TEST_NLATTR_ARRAY: a known attribute type. */
# define JSONL_NLATTR_ARRAY(fd_, nlh0_, hdrlen_,			\
			    init_msg_, print_msg_,			\
			    nla_type_, pattern_buf_, obj_, print_elem_)	\
	JSONL_NLATTR_ARRAY_((fd_), (nlh0_), (hdrlen_),			\
			    (init_msg_), (print_msg_),			\
			    (nla_type_), #nla_type_, NULL,		\
			    (pattern_buf_), obj_, (print_elem_))

/*
 * The JSONL counterparts of the check_*_nlattr functions: an integer
 * attribute that is too short, not entirely readable, of the exact size,
 * and followed by ignored data; print_val_ prints the fields of the value.
 */
# define DEF_JSONL_NLATTR_INTEGER_CHECK_(name_, type_, print_val_)	\
	static inline void						\
	jsonl_check_##name_##_nlattr(int fd, void *nlh0,		\
				     unsigned int hdrlen,		\
				     void (*init_msg)(struct nlmsghdr *, \
						      unsigned int),	\
				     void (*print_msg)(unsigned int),	\
				     unsigned int nla_type,		\
				     const char *sym,			\
				     const char *pattern,		\
				     const void *pattern_buf,		\
				     unsigned int depth)		\
	{								\
		static const type_ vecs[] = {				\
			(type_) 0,					\
			(type_) 1,					\
			(type_) 0xdeadfacebeeffeedULL,			\
		};							\
		static char buf[sizeof(type_) + 8];			\
		for (size_t i = 0; i < ARRAY_SIZE(vecs); i++) {		\
			JSONL_NESTED_NLATTR_OBJECT_EX_(fd, nlh0, hdrlen, \
				init_msg, print_msg,			\
				nla_type, sym, pattern,			\
				pattern_buf, vecs[i],			\
				jsonl_nl_bytes, depth,			\
				print_val_(vecs[i]));			\
			fill_memory(buf, sizeof(buf));			\
			memcpy(buf, vecs + i, sizeof(vecs[i]));		\
			JSONL_NESTED_NLATTR_(fd, nlh0, hdrlen,		\
				init_msg, print_msg,			\
				nla_type, sym, pattern,			\
				sizeof(buf), buf, sizeof(buf), depth,	\
				print_val_(vecs[i]));			\
		}							\
	}

DEF_JSONL_NLATTR_INTEGER_CHECK_(s32, int32_t, jsonl_decimal)

DEF_JSONL_NLATTR_INTEGER_CHECK_(u8, uint8_t, jsonl_unsigned)
DEF_JSONL_NLATTR_INTEGER_CHECK_(u16, uint16_t, jsonl_unsigned)
DEF_JSONL_NLATTR_INTEGER_CHECK_(u32, uint32_t, jsonl_unsigned)
DEF_JSONL_NLATTR_INTEGER_CHECK_(u64, uint64_t, jsonl_unsigned)

DEF_JSONL_NLATTR_INTEGER_CHECK_(x8, uint8_t, jsonl_hex)
DEF_JSONL_NLATTR_INTEGER_CHECK_(x16, uint16_t, jsonl_hex)
DEF_JSONL_NLATTR_INTEGER_CHECK_(x32, uint32_t, jsonl_hex)

/* The JSONL counterpart of TEST_NLATTR_VAL: a known attribute type. */
# define JSONL_NLATTR_VAL(type_, fd_, nlh0_, hdrlen_,			\
			  init_msg_, print_msg_,			\
			  nla_type_, pattern_buf_, depth_)		\
	jsonl_check_##type_##_nlattr((fd_), (nlh0_), (hdrlen_),		\
				     (init_msg_), (print_msg_),		\
				     (nla_type_), #nla_type_, NULL,	\
				     (pattern_buf_), (depth_))

#endif /* !STRACE_TESTS_JSONL_NETLINK_H */
