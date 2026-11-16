/*
 * Copyright (c) 2016-2017 Dmitry V. Levin <ldv@strace.io>
 * Copyright (c) 2017-2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef STRACE_PRINT_FIELDS_H
# define STRACE_PRINT_FIELDS_H

# include "gcc_compat.h"
# include "static_assert.h"
# include <stdarg.h>
# include <stdbool.h>
# include <string.h>

# ifdef IN_STRACE

#  include "color.h"

#  define STRACE_PRINTS(s_) tprints_string(s_)
#  define STRACE_PRINT_COLOR_SEQ(kind_) tprint_color_seq(kind_)

/*
 * The printf-like function to use in header files
 * shared between strace and its tests.
 */
#  define STRACE_PRINTF tprintf_string
#  define STRACE_PRINTV(fmt_, args_) tvprintf(fmt_, args_)

extern void tprint_xlat_flags_begin(uint64_t flags, const char *dflt,
				    enum xlat_style style);
extern void tprints_xlat_const(uint64_t val, enum xlat_style style,
			       const char *str);
extern void tprints_xlat_flags_sym(const char *sym, uint64_t val,
				   uint64_t flags, unsigned int n,
				   enum xlat_style style);
extern void tprint_sysret_xlat_val(unsigned long long val, bool hex,
				   const struct xlat *xlat);
extern void tprint_sysret_flags_val(unsigned long long val,
				    const struct xlat *xlat);
extern void tprint_sysret_printer_val(unsigned long long val,
				      void (*printer)(unsigned long long val,
						      enum xlat_style style));
extern bool tprint_xlat_flags_value_begin(unsigned long long raw,
					  enum xlat_style style);
extern void tprint_xlat_flags_value_end(enum xlat_style style);
extern void tprint_flags_with_value_begin(uint64_t raw, uint64_t flags,
					  const struct xlat *xlat,
					  const char *dflt);
extern void tprint_flags_with_value_end(uint64_t flags);
extern void tprint_wait_status(int status, const struct xlat *events);
extern void tprint_net_xval(const void *raw, unsigned int size,
			    const struct xlat *xlat, unsigned long long val,
			    const char *dflt);
extern void tprint_ax25_addr(const void *call, unsigned int size,
			     enum xlat_style style, const char *str);
extern void tprint_ioprio(unsigned int ioprio, bool signed_raw,
			  const char *str, const struct xlat *class_xlat,
			  unsigned int class, unsigned int level);
extern void tprint_sigmask_begin(const uint32_t *mask, unsigned int nwords,
				 bool inverted);
extern int tprint_xlat_flags_end(uint64_t orig_flags, uint64_t flags,
				 unsigned int n, const char *dflt,
				 enum xlat_style style);

# else /* !IN_STRACE */

#  include <stdio.h>

#  define STRACE_PRINTS(s_) fputs((s_), stdout)

/*
 * The printf-like function to use in header files
 * shared between strace and its tests.
 */
#  define STRACE_PRINTF printf
#  define STRACE_PRINTV(fmt_, args_) vprintf(fmt_, args_)

#  define COLOR_ARGNAME 0
#  define COLOR_ARGVAL 0
#  define COLOR_CALL 0
#  define COLOR_COMMENT 0
#  define COLOR_CONST 0
#  define COLOR_ERROR 0
#  define COLOR_PUNCT 0
#  define COLOR_RETVAL 0
#  define COLOR_SYSCALL 0

#  define COLOR_RESET 0

#  define STRACE_PRINT_COLOR_SEQ(kind_) do { } while (0)

#  define color_is_enabled 0

# endif /* !IN_STRACE */

# define structured_output 0

static inline void
tprint_event_end(void)
{
	STRACE_PRINT_COLOR_SEQ(COLOR_RESET);
}

static inline void
tprints_object_type(const char *type)
{
	(void) type;
}

static inline void ATTRIBUTE_FORMAT((printf, 2, 3))
tprintf_object_field_string(const char *field, const char *fmt, ...)
{
	va_list args;

	(void) field;
	va_start(args, fmt);
	STRACE_PRINTV(fmt, args);
	va_end(args);
}

static inline void
tprint_object_begin(void)
{
}

static inline void
tprint_object_end(void)
{
}

static inline void
tprints_object_field_bool(const char *field, bool val)
{
	(void) field;
	(void) val;
}

static inline void
tprints_object_field_null(const char *field)
{
	(void) field;
}

static inline void
tprint_type_const(void)
{
}

static inline void
tprint_list_begin(void)
{
}

static inline void
tprint_list_element_begin(void)
{
}

static inline void
tprint_list_element_end(void)
{
}

static inline void
tprint_list_end(void)
{
}

static inline void
tprints_object_field_begin(const char *field)
{
	(void) field;
}

static inline void
tprint_object_field_end(void)
{
}

static inline void
tprints_object_field_string(const char *field, const char *s)
{
	(void) field;
	STRACE_PRINTS(s);
}

static inline void
tprints_string_value(const char *str)
{
	STRACE_PRINTS(str);
}

static inline bool
tprint_struct_fields_end(void)
{
	return false;
}

static inline void
tprint_flags_element_mark(void)
{
}

static inline bool
tprint_flags_element_open(void)
{
	return false;
}

extern void tprint_struct_begin(void);
extern void tprint_struct_next(void);
extern void tprint_struct_end(void);
extern void tprint_struct_decode_error(const char *msg);
extern void tprint_struct_addr(unsigned long long addr);
extern void tprint_union_begin(void);
extern void tprint_union_next(void);
extern void tprint_union_end(void);
extern void tprint_array_begin(void);
extern void tprint_pair_begin(void);
extern void tprint_array_next(void);
extern void tprint_array_end(void);

static inline void
tprint_array_element_next(void)
{
	tprint_object_end();
	tprint_array_next();
	tprint_object_begin();
}

extern void tprint_array_element_fetch_failed(unsigned long long addr);
extern void tprint_array_index_begin(void);
extern void tprint_array_index_equal(void);
extern void tprints_syscall_name(const char *name);
extern void tprints_arg_begin(const char *name);
extern void tprint_arg_next(void);
extern void tprint_arg_end(void);
extern void tprints_arg_name_unconditionally(const char *name);
extern void tprints_arg_next_name_unconditionally(const char *name);
extern void tprints_arg_name(const char *name);
extern void tprints_arg_next_name(const char *name);
extern void tprints_fn_begin(const char *name);
extern void tprint_fn_next(void);
extern void tprint_fn_end(void);

extern void tprint_fd_set_begin(void);
extern void tprint_fd_set_next(void);
extern void tprint_fd_set_end(void);
extern void tprint_bitset_begin(void);
extern void tprint_bitset_next(void);
extern void tprint_bitset_end(void);
extern void tprint_cpu_set_begin(void);
extern void tprint_cpu_set_next(void);
extern void tprint_cpu_set_cpu(int cpu);
extern void tprint_cpu_set_end(void);
extern bool tprint_bitset_mask_begin(const void *buf, unsigned int size);
extern void tprint_bitset_raw_begin(unsigned int raw, bool inverted);
extern void tprint_bitset_mask_next(void);
extern void tprint_bitset_mask_end(void);
extern bool tprint_bitset_words_begin(const void *buf, unsigned int size);
extern void tprint_bitset_words_end(void);
extern void tprint_comment_begin(void);
extern void tprint_comment_end(void);
extern void tprint_indirect_begin(void);
extern void tprint_indirect_end(void);
extern void tprint_attribute_begin(void);
extern void tprint_attribute_end(void);
extern void tprint_associated_info_begin(void);
extern void tprint_associated_info_end(void);
extern void tprints_fd_info_string(const char *type, const char *str);
extern void tprint_fd_info_dev(const char *path, const char *kind,
			       unsigned int major, unsigned int minor,
			       int tty_index, const char *tun_backend);
extern void tprint_fd_info_pidfd(int pid);
extern void tprints_fd_info_signalfd(const char *sigmask);
extern void tprint_fd_info_eventfd(const char *count, int id, int semaphore);
extern void tprints_assoc_type_string(const char *type, const char *str,
				      size_t len, bool deleted);
extern void tprints_pid_info(const char *comm, int strace_pid);
extern void tprint_more_data_follows(void);
extern void tprint_value_changed(void);
extern void tprint_alternative_value(void);
extern void tprint_unavailable(void);
extern void tprint_unavailable_error(const char *msg);
extern void tprint_flags_or(void);
extern void tprint_newline(void);
extern void tprints_field_name(const char *name);
extern void tprints_unnamed_field_name(const char *name);
extern void tprints_xlat_sym(const char *str);
extern void tprints_xlat_dflt(const char *dflt);
extern void tprint_sockaddr_begin(void);
extern void tprint_sockaddr_family_begin(void);
extern void tprint_sockaddr_family_end(void);
extern void tprint_sockaddr_next(void);
extern void tprint_sockaddr_end(void);
extern void tprint_sockaddr_in(const char *addr, unsigned int port,
			       void (*print_struct_fields)(const void *),
			       const void *data);
extern void tprint_sockaddr_in6(const char *addr, unsigned int port,
				unsigned int flowinfo, bool has_scope_id,
				unsigned int scope_id, bool scope_id_ifindex,
				void (*print_struct_fields)(const void *),
				const void *data);
extern void tprint_sockaddr_nl(unsigned int pad, unsigned int pid,
			       unsigned int groups,
			       void (*print_struct_fields)(const void *),
			       const void *data);
extern void tprint_sun_path_name(void);
extern void tprint_sun_path_abstract(void);
extern void tprints_quoted_string(const char *quoted, const char *raw,
				  unsigned int size, bool nul_terminated);
extern void tprint_sysret_begin(void);
extern void tprints_sysret_next(const char *name);
extern void tprints_sysret_string(const char *name, const char *str);
extern void tprints_sysret_xlat_sym(const char *sym);
extern void tprint_sysret_result_begin(unsigned long long val);
extern void tprints_sysret_result_field(const char *name, bool labeled);
extern void tprints_sysret_result_note(const char *note);
extern void tprints_sysret_result_sym(const char *sym);
extern void tprint_sysret_result_end(void);
extern void tprint_fd_num(int fd);
extern void tprint_port_range(unsigned int ports);
extern void tprint_negated_begin(long long raw);
extern void tprint_negated_end(void);
extern void tprint_sigmask_next(void);
extern void tprint_sigmask_end(void);
extern void tprint_sigmask_member(unsigned int sig, const char *name);
extern void tprint_numeric_mode(unsigned long long mode);
extern void tprint_symbolic_mode(unsigned int mode, const char *ifmt);
extern void tprint_dev_t(unsigned long long dev, unsigned int dev_major,
			 unsigned int dev_minor);
extern void tprint_err(long long err, bool negated, const char *name);
extern void tprint_ifindex(unsigned int ifindex, const char *name);
extern void tprint_ip_addr(const char *var_name, bool v6, const void *addr,
			   unsigned int len, const char *str);
extern void tprint_flags_groups_begin(unsigned long long raw);
extern void tprint_flags_groups_next(void);
extern void tprint_flags_groups_end(void);
extern void tprint_flags_group_begin(unsigned long long raw);
extern void tprint_flags_shift_group_begin(unsigned long long raw);
extern void tprint_flags_mode_group_begin(unsigned long long raw);
extern void tprint_flags_group_end(void);
extern void tprint_flags_value_begin(unsigned long long raw);
extern void tprint_flags_value_or(void);
extern void tprint_flags_value_end(void);
extern void tprints_flags_value_const(unsigned long long val,
				      const char *sym);
extern void tprints_shift_value(unsigned long long raw, unsigned long long val,
				const char *shift);
extern void tprints_shift_value_xlat_u(unsigned long long raw,
				      unsigned long long val,
				      unsigned int amount, const char *shift);
extern void tprint_shift_value_begin(unsigned long long raw);
extern void tprint_shift_value_end_u(unsigned int amount);
extern void tprints_shift_value_end(const char *shift);
extern void tprint_mac_addr(const void *addr, unsigned int size,
			    const char *str);
extern void tprint_fn_hton_begin(unsigned int size);
extern void tprint_fn_hton_end(unsigned int size);
extern void tprint_net_port(const void *raw, unsigned int port);
extern void tprint_hton_xlat(unsigned long long stored, unsigned long long val,
			     const char *sym, unsigned int size);
extern void tprint_addr(unsigned long long addr);
extern void tprint_secontext(const char *ctx, unsigned int ctx_len,
			     const char *expected, unsigned int expected_len,
			     unsigned int style);
extern void tprint_secontext_pid(const char *ctx, unsigned int ctx_len,
				 unsigned int style);
extern void tprint_var_count(unsigned long long addr, unsigned int count,
			     bool unterminated);
extern void tprints_uuid(const char *str);
extern void tprint_char(unsigned char c);
extern void tprint_timespec(long long sec, unsigned long long nsec);
extern void tprint_timespec_signed(long long sec, long long nsec);
extern void tprint_timespec_sym(long long sec, unsigned long long nsec,
				const char *sym);
extern void tprint_timeval(long long sec, unsigned long long usec);
extern void tprint_dirent_count_begin(unsigned long long addr);
extern void tprint_dirent_count_end(unsigned int count, bool truncated);
extern void tprint_extra_data_unavailable(void);
extern void tprint_extra_data_begin(unsigned int offset, unsigned int size);
extern void tprint_extra_data_end(bool truncated);
extern void tprints_timestamp_comment(const char *str);
extern void tprint_time_t(long long sec, const char *str);
extern void tprint_ticks(unsigned long long val, long freq,
			 unsigned int precision);
extern void tprint_ticks_d(long long val, long freq, unsigned int precision);
extern void tprint_kernel_version(unsigned long version);
extern bool tprint_qcmd_begin(unsigned int qcmd);
extern void tprint_rlim(unsigned long long lim, const char *str,
			bool infinity);
extern void tprint_syscall_nr(unsigned long long nr, const char *name,
			      const char *nr_prefix);
extern void tprint_clockid_fd(int clockid, unsigned int fd);
extern bool tprint_clockid_cpu_begin(int clockid, int pid, bool per_thread);
extern void tprint_clockid_cpu_end(void);
extern void tprint_clockid_aux(int clockid, unsigned int index);
extern void tprint_qcmd_next(void);
extern void tprint_qcmd_end(void);
extern void tprint_signal(int nr, const char *name);
extern void tprint_perf_precise_ip(unsigned int val, const char *desc,
				   const char *skid);
extern void tprint_reserved_bits(unsigned long long val, unsigned int from,
				 unsigned int to);
extern void tprint_unused_bits(unsigned long long val, unsigned int first,
			       unsigned int last);
extern void tprint_syscall_duration(long long sec, long nsec);
extern void tprint_sysret_end(void);
extern void tprint_sysret_errno(unsigned long u_error, const char *sym,
				const char *str);
extern void tprint_syscall_exit_line_end(void);
extern void tprint_syscall_exit_event_end(void);
extern void tprint_io_dump_begin(void);
extern void tprint_io_dump_line(const char *text, unsigned long long offset,
				int offset_width, const void *data,
				unsigned int size);
extern void tprint_io_dump_fetch_failed(unsigned long long size,
					unsigned long long fetch_size, int pid,
					unsigned long long addr,
					unsigned long long fetched);
extern void tprint_io_dump_end(void);
extern void tprint_io_dump_iov_begin(void);
extern void tprint_io_dump_iov_buffer(unsigned long long size,
				      unsigned int index);
extern void tprint_io_dump_iov_end(void);
extern void tprint_io_dump_mmsg_begin(void);
extern void tprint_io_dump_mmsg_vector_begin(unsigned long long iovlen,
					     unsigned int index);
extern void tprint_io_dump_mmsg_vector_end(void);
extern void tprint_io_dump_mmsg_end(void);

static inline void
tprint_array_index_end(void)
{
}

static inline void
tprint_shift_begin(void)
{
}

static inline void
tprint_shift_end(void)
{
}

static inline void
tprint_shift(void)
{
	STRACE_PRINTS("<<");
}

static inline void
tprint_flags_begin(void)
{
}

static inline void
tprint_flags_end(void)
{
}

static inline void
tprint_plus(void)
{
	STRACE_PRINTS("+");
}

static inline void
tprint_space(void)
{
	STRACE_PRINTS(" ");
}

static inline void
tprint_null(void)
{
	if (structured_output) {
		tprints_object_type("addr");
		tprints_object_field_null("raw");
	} else {
		STRACE_PRINTS("NULL");
	}
}

static inline void
tprint_sysret_pseudo_rval(void)
{
	STRACE_PRINTS("?");
}

static inline void
tprint_sysret_interrupted(void)
{
	STRACE_PRINTS("?");
}

# define PRINT_VAL_D(val_)						\
	do {								\
		tprints_object_type("decimal");				\
		tprintf_object_field_string("raw", "%lld",		\
			sign_extend_unsigned_to_ll(val_));		\
	} while (0)

# define PRINT_VAL_D_AS(type_, val_)					\
	do {								\
		tprints_object_type(type_);				\
		tprintf_object_field_string("raw", "%lld",		\
			sign_extend_unsigned_to_ll(val_));		\
	} while (0)

# define PRINT_VAL_U(val_)						\
	do {								\
		tprints_object_type("unsigned");			\
		tprintf_object_field_string("raw", "%llu",		\
			zero_extend_signed_to_ull(val_));		\
	} while (0)

# define PRINT_VAL_X(val_)						\
	do {								\
		tprints_object_type("hex");				\
		tprintf_object_field_string("raw", "%#llx",		\
			zero_extend_signed_to_ull(val_));		\
	} while (0)

# define PRINT_VAL_03O(val_)						\
	do {								\
		tprints_object_type("octal");				\
		tprintf_object_field_string("raw", "%#03llo",		\
			zero_extend_signed_to_ull(val_));		\
	} while (0)

# define PRINT_VAL_0X(val_)						\
	do {								\
		tprints_object_type("hex");				\
		if (structured_output)					\
			tprintf_object_field_string("raw", "%#llx",	\
				zero_extend_signed_to_ull(val_));	\
		else							\
			tprintf_object_field_string("raw", "%#0*llx",	\
				(int) sizeof(val_) * 2,			\
				zero_extend_signed_to_ull(val_));	\
	} while (0)

# define PRINT_VAL_ADDR(val_)						\
	do {								\
		tprints_object_type("addr");				\
		tprintf_object_field_string("raw", "%#llx",		\
			zero_extend_signed_to_ull(val_));		\
	} while (0)

# define PRINT_VAL_FD(val_)						\
	do {								\
		tprints_object_type("fd");				\
		tprintf_object_field_string("raw", "%d", (val_));	\
	} while (0)

# define PRINT_VAL_ID(val_)						\
	do {								\
		if (sign_extend_unsigned_to_ll(val_) == -1LL)		\
			PRINT_VAL_D(-1);				\
		else							\
			PRINT_VAL_U(val_);				\
	} while (0)

# define PRINT_VAL_ID_AS(type_, val_)					\
	do {								\
		tprints_object_type(type_);				\
		if (sign_extend_unsigned_to_ll(val_) == -1LL)		\
			tprintf_object_field_string("raw", "-1");	\
		else							\
			tprintf_object_field_string("raw", "%llu",	\
				zero_extend_signed_to_ull(val_));	\
	} while (0)

# define PRINT_VAL_UID(val_)	PRINT_VAL_ID_AS("uid", val_)
# define PRINT_VAL_GID(val_)	PRINT_VAL_ID_AS("gid", val_)

# define PRINT_FIELD_D(where_, field_)					\
	do {								\
		tprints_field_name(#field_);				\
		PRINT_VAL_D((where_).field_);				\
	} while (0)

# define PRINT_FIELD_U(where_, field_)					\
	do {								\
		tprints_field_name(#field_);				\
		PRINT_VAL_U((where_).field_);				\
	} while (0)

# define PRINT_FIELD_U_CAST(where_, field_, type_)			\
	do {								\
		tprints_field_name(#field_);				\
		PRINT_VAL_U((type_)((where_).field_));			\
	} while (0)

# define PRINT_FIELD_X(where_, field_)					\
	do {								\
		tprints_field_name(#field_);				\
		PRINT_VAL_X((where_).field_);				\
	} while (0)

# define PRINT_FIELD_X_CAST(where_, field_, type_)			\
	do {								\
		tprints_field_name(#field_);				\
		PRINT_VAL_X((type_)((where_).field_));			\
	} while (0)

# define PRINT_FIELD_ADDR64(where_, field_)				\
	do {								\
		tprints_field_name(#field_);				\
		printaddr64((where_).field_);				\
	} while (0)

# define PRINT_FIELD_0X(where_, field_)					\
	do {								\
		tprints_field_name(#field_);				\
		PRINT_VAL_0X((where_).field_);				\
	} while (0)

# define PRINT_FIELD_VAL_ARRAY(where_, field_, print_val_)		\
	do {								\
		tprints_field_name(#field_);				\
		for (size_t i_ = 0;					\
		     i_ < ARRAY_SIZE((where_).field_);			\
		     ++i_) {						\
			if (i_)						\
				tprint_array_next();			\
			else						\
				tprint_array_begin();			\
			tprint_object_begin();				\
			print_val_((where_).field_[i_]);		\
			tprint_object_end();				\
		}							\
		tprint_array_end();					\
	} while (0)

# define PRINT_FIELD_D_ARRAY(where_, field_)				\
	PRINT_FIELD_VAL_ARRAY((where_), field_, PRINT_VAL_D)

# define PRINT_FIELD_U_ARRAY(where_, field_)				\
	PRINT_FIELD_VAL_ARRAY((where_), field_, PRINT_VAL_U)

# define PRINT_FIELD_X_ARRAY(where_, field_)				\
	PRINT_FIELD_VAL_ARRAY((where_), field_, PRINT_VAL_X)

# define PRINT_FIELD_VAL_ARRAY2D(where_, field_, print_val_)		\
	do {								\
		tprints_field_name(#field_);				\
		for (size_t i_ = 0;					\
		     i_ < ARRAY_SIZE((where_).field_);			\
		     ++i_) {						\
			if (i_)						\
				tprint_array_next();			\
			else						\
				tprint_array_begin();			\
			tprint_object_begin();				\
			for (size_t j_ = 0;				\
			     j_ < ARRAY_SIZE((where_).field_[i_]);	\
			     ++j_) {					\
				if (j_)					\
					tprint_array_next();		\
				else					\
					tprint_array_begin();		\
				tprint_object_begin();			\
				print_val_((where_).field_[i_][j_]);	\
				tprint_object_end();			\
			}						\
			tprint_array_end();				\
			tprint_object_end();				\
		}							\
		tprint_array_end();					\
	} while (0)

# define PRINT_FIELD_X_ARRAY2D(where_, field_)				\
	PRINT_FIELD_VAL_ARRAY2D((where_), field_, PRINT_VAL_X)

# define PRINT_FIELD_COOKIE(where_, field_)				\
	do {								\
		static_assert(ARRAY_SIZE((where_).field_) == 2,		\
			      "unexpected array size");			\
		PRINT_FIELD_U_ARRAY((where_), field_);			\
	} while (0)

# define PRINT_FIELD_FLAGS(where_, field_, xlat_, dflt_)		\
	do {								\
		tprints_field_name(#field_);				\
		printflags64((xlat_),					\
			     zero_extend_signed_to_ull((where_).field_),\
			     (dflt_));					\
	} while (0)

# define PRINT_FIELD_FLAGS_VERBOSE(where_, field_, xlat_, dflt_)	\
	do {								\
		tprints_field_name(#field_);				\
		tprint_flags_begin();					\
		printflags_ex(zero_extend_signed_to_ull((where_).field_), \
			      (dflt_),					\
			      xlat_verbose(xlat_verbosity) == XLAT_STYLE_RAW \
				? XLAT_STYLE_RAW : XLAT_STYLE_VERBOSE,	\
			      (xlat_), NULL);				\
		tprint_flags_end();					\
	} while (0)

# define PRINT_FIELD_XVAL(where_, field_, xlat_, dflt_)			\
	do {								\
		tprints_field_name(#field_);				\
		printxval64((xlat_),					\
			    zero_extend_signed_to_ull((where_).field_),	\
			    (dflt_));					\
	} while (0)

# define PRINT_FIELD_XVAL_VERBOSE(where_, field_, xlat_, dflt_)		\
	do {								\
		tprints_field_name(#field_);				\
		printxval_ex((xlat_),					\
			     zero_extend_signed_to_ull((where_).field_), \
			     (dflt_),					\
			     xlat_verbose(xlat_verbosity) == XLAT_STYLE_RAW \
				? XLAT_STYLE_RAW : XLAT_STYLE_VERBOSE);	\
	} while (0)

# define PRINT_FIELD_XVAL_D(where_, field_, xlat_, dflt_)		\
	do {								\
		tprints_field_name(#field_);				\
		printxval64_d((xlat_),					\
			      sign_extend_unsigned_to_ll((where_).field_), \
			      (dflt_));					\
	} while (0)

# define PRINT_FIELD_XVAL_U(where_, field_, xlat_, dflt_)		\
	do {								\
		tprints_field_name(#field_);				\
		printxval64_u((xlat_),					\
			      zero_extend_signed_to_ull((where_).field_), \
			      (dflt_));					\
	} while (0)

# define PRINT_FIELD_XVAL_U_VERBOSE(where_, field_, xlat_, dflt_)	\
	do {								\
		tprints_field_name(#field_);				\
		printxval_ex((xlat_),					\
			     zero_extend_signed_to_ull((where_).field_), \
			     (dflt_),					\
			     (xlat_verbose(xlat_verbosity) == XLAT_STYLE_RAW \
				? XLAT_STYLE_RAW : XLAT_STYLE_VERBOSE)	\
			     | XLAT_STYLE_FMT_U);			\
	} while (0)

# define PRINT_FIELD_ERR_D(where_, field_)				\
	do {								\
		tprints_field_name(#field_);				\
		print_err(sign_extend_unsigned_to_ll((where_).field_),	\
			  true);					\
	} while (0)

# define PRINT_FIELD_ERR_U(where_, field_)				\
	do {								\
		tprints_field_name(#field_);				\
		print_err(zero_extend_signed_to_ull((where_).field_),	\
			  false);					\
	} while (0)

/*
 * Generic "ID" printing. ID is considered unsigned except for the special value
 * of -1.
 */
# define PRINT_FIELD_ID(where_, field_)					\
	do {								\
		tprints_field_name(#field_);				\
		PRINT_VAL_ID((where_).field_);				\
	} while (0)

# define PRINT_FIELD_UID(where_, field_)				\
	do {								\
		tprints_field_name(#field_);				\
		PRINT_VAL_UID((where_).field_);				\
	} while (0)

# define PRINT_FIELD_GID(where_, field_)				\
	do {								\
		tprints_field_name(#field_);				\
		PRINT_VAL_GID((where_).field_);				\
	} while (0)

# define PRINT_FIELD_UUID(where_, field_)				\
	do {								\
		tprints_field_name(#field_);				\
		print_uuid((const unsigned char *) ((where_).field_));	\
	} while (0)

# define PRINT_FIELD_U64(where_, field_)					\
	do {									\
		tprints_field_name(#field_);					\
		if (zero_extend_signed_to_ull((where_).field_) == UINT64_MAX)	\
			print_xlat_u(UINT64_MAX);				\
		else								\
			PRINT_VAL_U((where_).field_);				\
	} while (0)

# define PRINT_FIELD_TICKS(where_, field_, freq_, precision_)		\
	do {								\
		tprints_field_name(#field_);				\
		print_ticks((where_).field_, (freq_), (precision_));	\
	} while (0)

# define PRINT_FIELD_TICKS_D(where_, field_, freq_, precision_)		\
	do {								\
		tprints_field_name(#field_);				\
		print_ticks_d((where_).field_, (freq_), (precision_));	\
	} while (0)

# define PRINT_FIELD_CLOCK_T(where_, field_)				\
	do {								\
		tprints_field_name(#field_);				\
		print_clock_t((where_).field_);				\
	} while (0)

# define PRINT_FIELD_TIME_T(where_, field_)				\
	do {								\
		tprints_field_name(#field_);				\
		print_time_t((where_).field_);				\
	} while (0)

# define PRINT_FIELD_STRING(where_, field_, len_, style_)		\
	do {								\
		tprints_field_name(#field_);				\
		tprints_object_type("string");				\
		print_quoted_string((const char *)(where_).field_,	\
				    (len_), (style_));			\
	} while (0)

# define PRINT_FIELD_CSTRING(where_, field_)				\
	do {								\
		tprints_field_name(#field_);				\
		tprints_object_type("string");				\
		print_quoted_cstring((const char *) (where_).field_,	\
				     sizeof((where_).field_) +		\
				     MUST_BE_ARRAY((where_).field_));	\
	} while (0)

# define PRINT_FIELD_CSTRING_SZ(where_, field_, size_)			\
	do {								\
		tprints_field_name(#field_);				\
		tprints_object_type("string");				\
		print_quoted_cstring((const char *) (where_).field_,	\
				     (size_));				\
	} while (0)

# define PRINT_FIELD_ARRAY(where_, field_, tcp_, print_func_)		\
	do {								\
		tprints_field_name(#field_);				\
		print_local_array((tcp_), (where_).field_,		\
				  (print_func_));			\
	} while (0)

# define PRINT_FIELD_ARRAY_INDEXED(where_, field_, tcp_, print_func_,	\
				   ind_xlat_, ind_dflt_)		\
	do {								\
		tprints_field_name(#field_);				\
		print_local_array_ex((tcp_), (where_).field_,		\
				  ARRAY_SIZE((where_).field_),		\
				  sizeof(((where_).field_)[0]),		\
				  (print_func_),			\
				  NULL, PAF_PRINT_INDICES | XLAT_STYLE_FMT_U, \
				  (ind_xlat_), (ind_dflt_));		\
	} while (0)

# define PRINT_FIELD_ARRAY_UPTO(where_, field_,				\
				upto_, tcp_, print_func_)		\
	do {								\
		tprints_field_name(#field_);				\
		print_local_array_upto((tcp_), (where_).field_,		\
				       (upto_), (print_func_));		\
	} while (0)

# define PRINT_FIELD_HEX_ARRAY(where_, field_)				\
	do {								\
		tprints_field_name(#field_);				\
		tprints_object_type("bytes");				\
		print_quoted_string((const char *)(where_).field_,	\
				    sizeof((where_).field_) +		\
					    MUST_BE_ARRAY((where_).field_), \
				    QUOTE_FORCE_HEX); \
	} while (0)

# define PRINT_FIELD_HEX_ARRAY_UPTO(where_, field_, upto_)		\
	do {								\
		tprints_field_name(#field_);				\
		tprints_object_type("bytes");				\
		print_quoted_string((const char *)(where_).field_,	\
				    (upto_), QUOTE_FORCE_HEX);		\
	} while (0)

# define PRINT_FIELD_INET_ADDR(where_, field_, af_)			\
	print_inet_addr((af_), &(where_).field_,			\
			sizeof((where_).field_), #field_)

# define PRINT_FIELD_NET_PORT(where_, field_)				\
	do {								\
		static_assert(sizeof((where_).field_) == 2,		\
			      "unexpected size of " #field_);		\
		tprints_field_name(#field_);				\
		tprint_net_port(&(where_).field_,			\
				ntohs((where_).field_));		\
	} while (0)

# define PRINT_FIELD_IFINDEX(where_, field_)				\
	do {								\
		tprints_field_name(#field_);				\
		print_ifindex((where_).field_);				\
	} while (0)

# define PRINT_FIELD_SOCKADDR(where_, field_, tcp_)			\
	do {								\
		tprints_field_name(#field_);				\
		print_sockaddr(tcp_, &(where_).field_,			\
			       sizeof((where_).field_));		\
	} while (0)

# define PRINT_FIELD_DEV(where_, field_)				\
	do {								\
		tprints_field_name(#field_);				\
		print_dev_t((where_).field_);				\
	} while (0)

# define PRINT_FIELD_PTR(where_, field_)				\
	do {								\
		tprints_field_name(#field_);				\
		printaddr((mpers_ptr_t) (where_).field_);		\
	} while (0)

# define PRINT_FIELD_FD(where_, field_, tcp_)				\
	do {								\
		tprints_field_name(#field_);				\
		printfd((tcp_), (where_).field_);			\
	} while (0)

# define PRINT_FIELD_DIRFD(where_, field_, tcp_)			\
	do {								\
		tprints_field_name(#field_);				\
		print_dirfd((tcp_), (where_).field_);			\
	} while (0)

# define PRINT_FIELD_CHAR(where_, field_)				\
	do {								\
		tprints_field_name(#field_);				\
		tprint_char((where_).field_);				\
	} while (0)

# define PRINT_FIELD_TGID(where_, field_, tcp_)				\
	do {								\
		tprints_field_name(#field_);				\
		printpid((tcp_), (where_).field_, PT_TGID);		\
	} while (0)

# define PRINT_FIELD_TID(where_, field_, tcp_)				\
	do {								\
		tprints_field_name(#field_);				\
		printpid((tcp_), (where_).field_, PT_TID);		\
	} while (0)

# define PRINT_FIELD_SYSCALL_NAME(where_, field_, audit_arch_)		\
	do {								\
		tprints_field_name(#field_);				\
		const char *nr_prefix_ = NULL;				\
		const unsigned long long ull_nr =			\
			zero_extend_signed_to_ull((where_).field_);	\
		const char *name = syscall_name_arch(ull_nr,		\
					(audit_arch_), &nr_prefix_);	\
		tprint_syscall_nr(ull_nr, name, nr_prefix_);		\
	} while (0)

# define PRINT_FIELD_MAC(where_, field_)				\
	PRINT_FIELD_MAC_SZ((where_), field_, ARRAY_SIZE((where_).field_))

# define PRINT_FIELD_MAC_SZ(where_, field_, size_)			\
	do {								\
		static_assert(sizeof(((where_).field_)[0]) == 1,	\
			      "MAC address is not a byte array");	\
		tprints_field_name(#field_);				\
		print_mac_addr("", (const uint8_t *) ((where_).field_),	\
			MIN((size_), ARRAY_SIZE((where_).field_)));	\
	} while (0)

# define PRINT_FIELD_HWADDR_SZ(where_, field_, size_, hwtype_)		\
	do {								\
		static_assert(sizeof(((where_).field_)[0]) == 1,	\
			      "hwaddress is not a byte array");		\
		tprints_field_name(#field_);				\
		print_hwaddr("", (const uint8_t *) ((where_).field_),	\
			       (size_), (hwtype_));			\
	} while (0)


# define PRINT_FIELD_OBJ_PTR(where_, field_, print_func_, ...)		\
	do {								\
		tprints_field_name(#field_);				\
		(print_func_)(&((where_).field_), ##__VA_ARGS__);	\
	} while (0)

# define PRINT_FIELD_OBJ_TCB_PTR(where_, field_,			\
				 tcp_, print_func_, ...)		\
	do {								\
		tprints_field_name(#field_);				\
		(print_func_)((tcp_), &((where_).field_),		\
			      ##__VA_ARGS__);				\
	} while (0)

# define PRINT_FIELD_OBJ_VAL(where_, field_, print_func_, ...)		\
	do {								\
		tprints_field_name(#field_);				\
		(print_func_)((where_).field_, ##__VA_ARGS__);		\
	} while (0)

# define PRINT_FIELD_OBJ_U(where_, field_, print_func_, ...)		\
	do {								\
		tprints_field_name(#field_);				\
		(print_func_)(zero_extend_signed_to_ull((where_).field_),\
			      ##__VA_ARGS__);				\
	} while (0)

# define PRINT_FIELD_OBJ_TCB_VAL(where_, field_,			\
			     tcp_, print_func_, ...)			\
	do {								\
		tprints_field_name(#field_);				\
		(print_func_)((tcp_), (where_).field_, ##__VA_ARGS__);	\
	} while (0)


# define MAYBE_PRINT_FIELD_LEN(print_prefix_, where_, field_,		\
			len_, print_func_, ...)				\
	do {								\
		unsigned int start = offsetof(typeof(where_), field_);	\
		unsigned int end = start + sizeof((where_).field_);	\
		if (len_ > start) {					\
			print_prefix_;					\
			if (len_ >= end) {				\
				print_func_((where_), field_,		\
					    ##__VA_ARGS__);		\
			} else {					\
				tprints_field_name(#field_);		\
				tprints_object_type("bytes");		\
				print_quoted_string(			\
					(void *)&(where_).field_,	\
					len_ - start, QUOTE_FORCE_HEX);	\
			}						\
		}							\
	} while (0)

#endif /* !STRACE_PRINT_FIELDS_H */
