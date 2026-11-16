/*
 * Copyright (c) 2016 Fabien Siron <fabien.siron@epita.fr>
 * Copyright (c) 2017 JingPiao Chen <chenjingpiao@gmail.com>
 * Copyright (c) 2016-2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"
#include <endian.h>
#include "netlink.h"
#include "nlattr.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/sock_diag.h>
#include "static_assert.h"

#include "xlat/netlink_sk_meminfo_indices.h"

static bool
fetch_nlattr(struct tcb *const tcp, struct nlattr *const nlattr,
	     const kernel_ulong_t addr, const unsigned int len,
	     const bool in_array)
{
	if (len < sizeof(struct nlattr)) {
		if (in_array)
			tprint_object_begin();
		printstr_ex(tcp, addr, len, QUOTE_FORCE_HEX);
		if (in_array)
			tprint_object_end();
		return false;
	}

	if (tfetch_obj(tcp, addr, nlattr))
		return true;

	if (in_array) {
		tprint_array_element_fetch_failed(addr);
	} else {
		printaddr(addr);
	}

	return false;
}

static void
print_nlattr(const struct nlattr *const nla,
	     const struct xlat *const table,
	     const char *const dflt)
{
	static_assert(NLA_TYPE_MASK == ~(NLA_F_NESTED | NLA_F_NET_BYTEORDER),
		      "wrong NLA_TYPE_MASK");

	tprint_struct_begin();
	PRINT_FIELD_U(*nla, nla_len);
	tprint_struct_next();
	tprints_field_name("nla_type");
	if (nla->nla_type & ~NLA_TYPE_MASK) {
		tprint_flags_value_begin(nla->nla_type);
		if (nla->nla_type & NLA_F_NESTED) {
			print_xlat(NLA_F_NESTED);
			tprint_flags_value_or();
		}
		if (nla->nla_type & NLA_F_NET_BYTEORDER) {
			print_xlat(NLA_F_NET_BYTEORDER);
			tprint_flags_value_or();
		}
		printxval(table, nla->nla_type & NLA_TYPE_MASK, dflt);
		tprint_flags_value_end();
	} else {
		printxval(table, nla->nla_type, dflt);
	}
	tprint_struct_end();
}

static void
decode_nlattr_with_data(struct tcb *const tcp,
			const struct nlattr *const nla,
			const kernel_ulong_t addr,
			const unsigned int len,
			const struct xlat *const table,
			const char *const dflt,
			const nla_decoder_t *const decoders,
			const unsigned int size,
			const void *const opaque_data)
{
	const unsigned int nla_len = MIN(nla->nla_len, len);
	const bool use_type = (size != -1U);

	if (nla_len > NLA_HDRLEN) {
		tprint_array_begin();
		tprint_object_begin();
	}

	print_nlattr(nla, table, dflt);

	if (nla_len > NLA_HDRLEN) {
		const unsigned int idx =
			(use_type && size) ? nla->nla_type & NLA_TYPE_MASK : 0;

		tprint_array_element_next();
		if (!decoders
		    || (use_type && size && idx >= size)
		    || !decoders[idx]
		    || !decoders[idx](
				tcp, addr + NLA_HDRLEN,
				nla_len - NLA_HDRLEN,
				size ? opaque_data
				     : (const void *) (uintptr_t) nla->nla_type)
		    )
			printstr_ex(tcp, addr + NLA_HDRLEN,
				    nla_len - NLA_HDRLEN, QUOTE_FORCE_HEX);
		tprint_object_end();
		tprint_array_end();
	}
}

void
decode_nlattr(struct tcb *const tcp,
	      kernel_ulong_t addr,
	      unsigned int len,
	      const struct xlat *const table,
	      const char *const dflt,
	      const nla_decoder_t *const decoders,
	      const unsigned int size,
	      const void *const opaque_data)
{
	struct nlattr nla;
	bool is_array = false;

	if (decoders && !size && opaque_data)
		error_func_msg("[xlat %p, dflt \"%s\", decoders %p] "
			       "size is zero (going to pass nla_type as "
			       "decoder argument), but opaque data (%p) is not "
			       "- will be ignored",
			       table, dflt, decoders, opaque_data);

	for (unsigned int elt = 0;
	     fetch_nlattr(tcp, &nla, addr, len, is_array);
	     ++elt) {
		/* elt starts with 0, hence elt + 1 */
		if (sequence_truncation_needed(tcp, elt + 1)) {
			tprint_more_data_follows();
			break;
		}

		const unsigned int nla_len = NLA_ALIGN(nla.nla_len);
		kernel_ulong_t next_addr = 0;
		unsigned int next_len = 0;

		if (nla.nla_len >= NLA_HDRLEN) {
			next_len = (len >= nla_len) ? len - nla_len : 0;

			if (next_len && addr + nla_len > addr)
				next_addr = addr + nla_len;
		}

		if (!is_array && next_addr) {
			tprint_array_begin();
			is_array = true;
		}

		if (is_array)
			tprint_object_begin();
		decode_nlattr_with_data(tcp, &nla, addr, len, table, dflt,
					decoders, size, opaque_data);
		if (is_array)
			tprint_object_end();

		if (!next_addr)
			break;

		tprint_array_next();
		addr = next_addr;
		len = next_len;
	}

	if (is_array) {
		tprint_array_end();
	}
}

DECL_NLA(str)
{
	printstr_ex(tcp, addr, len,
		    QUOTE_OMIT_TRAILING_0 | QUOTE_EXPECT_TRAILING_0);

	return true;
}

DECL_NLA(strn)
{
	printstrn(tcp, addr, len);

	return true;
}

DECL_NLA(meminfo)
{
	uint32_t mem;
	const size_t nmemb = len / sizeof(mem);

	if (!nmemb)
		return false;

	unsigned int count = 0;
	print_array_ex(tcp, addr, nmemb, &mem, sizeof(mem),
		       tfetch_mem, print_uint_array_member, &count,
		       PAF_PRINT_INDICES | XLAT_STYLE_FMT_U,
		       netlink_sk_meminfo_indices, "SK_MEMINFO_???");

	return true;
}

DECL_NLA(fd)
{
	int fd;

	if (len < sizeof(fd))
		return false;
	else if (!umove_or_printaddr(tcp, addr, &fd))
		printfd(tcp, fd);

	return true;
}

DECL_NLA(uid)
{
	uint32_t uid;

	if (len < sizeof(uid))
		return false;
	else if (!umove_or_printaddr(tcp, addr, &uid))
		printuid(uid);

	return true;
}

DECL_NLA(gid)
{
	uint32_t gid;

	if (len < sizeof(gid))
		return false;
	else if (!umove_or_printaddr(tcp, addr, &gid))
		printgid(gid);

	return true;
}

DECL_NLA(clock_t)
{
	uint64_t val;

	if (len > sizeof(val))
		return false;

	if (!umoven_to_uint64_or_printaddr(tcp, addr, len, &val))
		print_clock_t(val);

	return true;
}

DECL_NLA(ifindex)
{
	uint32_t ifindex;

	if (len < sizeof(ifindex))
		return false;
	else if (!umove_or_printaddr(tcp, addr, &ifindex))
		print_ifindex(ifindex);

	return true;
}

DECL_NLA(xval)
{
	const struct decode_nla_xlat_opts * const opts = opaque_data;
	uint64_t data;

	if (len > sizeof(data) || len < opts->size)
		return false;

	const unsigned int fetch_len = opts->size ? MIN(len, opts->size) : len;

	if (!umoven_to_uint64_or_printaddr(tcp, addr, fetch_len, &data)) {
		if (opts->process_fn)
			data = opts->process_fn(data);
		if (opts->hton_size)
			tprint_fn_hton_begin(opts->hton_size);
		printxval_ex(opts->xlat, data, opts->dflt, opts->style);
		if (opts->hton_size)
			tprint_fn_hton_end(opts->hton_size);
	}

	return true;
}

static uint64_t
process_host_order(uint64_t val)
{
	return ntohs(val);
}

DECL_NLA(ether_proto)
{
	static const struct decode_nla_xlat_opts opts = {
		.xlat = ethernet_protocols,
		.dflt = "ETHER_P_???",
		.size = 2,
		.process_fn = process_host_order,
		.hton_size  = 2,
	};

	return decode_nla_xval(tcp, addr, len, &opts);
}

DECL_NLA(ip_proto)
{
	static const struct decode_nla_xlat_opts opts = {
		.xlat = inet_protocols,
		.dflt = "IPPROTO_???",
	};

	return decode_nla_xval(tcp, addr, len, &opts);
}

DECL_NLA(hwaddr)
{
	if (len > MAX_ADDR_LEN)
		return false;

	uint8_t buf[len];
	const uintptr_t arphrd = (uintptr_t) opaque_data;

	if (!umoven_or_printaddr(tcp, addr, len, buf)) {
		print_hwaddr("", buf, len, arphrd & NLA_HWADDR_FAMILY_OFFSET
				? arphrd & ~NLA_HWADDR_FAMILY_OFFSET : -1U);
	}

	return true;
}

DECL_NLA(in_addr)
{
	struct in_addr in;

	if (len < sizeof(in))
		return false;
	else if (!umove_or_printaddr(tcp, addr, &in))
		print_inet_addr(AF_INET, &in, sizeof(in), NULL);

	return true;
}

DECL_NLA(in6_addr)
{
	struct in6_addr in6;

	if (len < sizeof(in6))
		return false;
	else if (!umove_or_printaddr(tcp, addr, &in6))
		print_inet_addr(AF_INET6, &in6, sizeof(in6), NULL);

	return true;
}

DECL_NLA(flags)
{
	const struct decode_nla_xlat_opts * const opts = opaque_data;
	uint64_t data;

	if (len > sizeof(data) || len < opts->size)
		return false;

	const unsigned int fetch_len = opts->size ? MIN(len, opts->size) : len;

	if (!umoven_to_uint64_or_printaddr(tcp, addr, fetch_len, &data)) {
		if (opts->process_fn)
			data = opts->process_fn(data);
		if (opts->hton_size)
			tprint_fn_hton_begin(opts->hton_size);
		tprint_flags_begin();
		printflags_ex(data, opts->dflt, opts->style, opts->xlat, NULL);
		tprint_flags_end();
		if (opts->hton_size)
			tprint_fn_hton_end(opts->hton_size);
	}

	return true;
}

void
decode_nla_af_spec(struct tcb *const tcp,
		   const kernel_ulong_t addr,
		   const unsigned int len,
		   uint8_t af,
		   const struct af_spec_decoder_desc *descs,
		   size_t desc_cnt)
{
	const struct af_spec_decoder_desc *const descs_end = descs + desc_cnt;
	const struct af_spec_decoder_desc *desc = descs;

	for (; desc < descs_end; desc++) {
		if (desc->af == af)
			break;
	}

	if (desc >= descs_end) {
		printstr_ex(tcp, addr, len, QUOTE_FORCE_HEX);

	} else {
		decode_nlattr(tcp, addr, len,
			      desc->xlat, desc->dflt, desc->table, desc->size,
			      NULL);
	}
}


DECL_NLA(be16)
{
	uint16_t num;

	if (len < sizeof(num))
		return false;
	else if (!umove_or_printaddr(tcp, addr, &num)) {
		tprint_fn_hton_begin(2);
		PRINT_VAL_U(ntohs(num));
		tprint_fn_hton_end(2);
	}

	return true;
}

DECL_NLA(be32)
{
	unsigned int num;

	if (len < sizeof(num))
		return false;
	else if (!umove_or_printaddr(tcp, addr, &num)) {
		tprint_fn_hton_begin(4);
		PRINT_VAL_U(ntohl(num));
		tprint_fn_hton_end(4);
	}

	return true;
}

DECL_NLA(be64)
{
#if defined HAVE_BE64TOH || defined be64toh
	uint64_t num;

	if (len < sizeof(num))
		return false;
	else if (!umove_or_printaddr(tcp, addr, &num)) {
		tprint_fn_hton_begin(8);
		PRINT_VAL_U(be64toh(num));
		tprint_fn_hton_end(8);
	}

	return true;
#else
	return false;
#endif
}

#define DECODE_NLA_INTEGER(name, type, fmt)		\
DECL_NLA(name)						\
{							\
	type num;					\
							\
	if (len < sizeof(num))				\
		return false;				\
	if (!umove_or_printaddr(tcp, addr, &num))	\
		tprintf_string(fmt, num);		\
	return true;					\
}

DECODE_NLA_INTEGER(x8, uint8_t, "%#" PRIx8)
DECODE_NLA_INTEGER(x16, uint16_t, "%#" PRIx16)
DECODE_NLA_INTEGER(x32, uint32_t, "%#" PRIx32)
DECODE_NLA_INTEGER(x64, uint64_t, "%#" PRIx64)
DECODE_NLA_INTEGER(u8, uint8_t, "%" PRIu8)
DECODE_NLA_INTEGER(u16, uint16_t, "%" PRIu16)
DECODE_NLA_INTEGER(u32, uint32_t, "%" PRIu32)
DECODE_NLA_INTEGER(u64, uint64_t, "%" PRIu64)
DECODE_NLA_INTEGER(s8, int8_t, "%" PRId8)
DECODE_NLA_INTEGER(s16, int16_t, "%" PRId16)
DECODE_NLA_INTEGER(s32, int32_t, "%" PRId32)
DECODE_NLA_INTEGER(s64, int64_t, "%" PRId64)

DECL_NLA(xint)
{
	nla_decoder_t f = NULL;

	switch (len) {
	case sizeof(uint8_t):  f = decode_nla_x8;  break;
	case sizeof(uint16_t): f = decode_nla_x16; break;
	case sizeof(uint32_t): f = decode_nla_x32; break;
	case sizeof(uint64_t): f = decode_nla_x64; break;
	}

	if (f)
		return f(tcp, addr, len, opaque_data);

	return false;
}
