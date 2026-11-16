/*
 * Check JSONL output of NETLINK_CRYPTO messages.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_sendto

# include "jsonl_netlink.h"

# include <linux/cryptouser.h>

# define XLAT_MACROS_ONLY
#  include "xlat/netlink_protocols.h"
# undef XLAT_MACROS_ONLY

static const struct strval32 nlm_f_request[] = {
	{ ARG_STR(NLM_F_REQUEST) }
};

static void
test_hdr(const int fd, const unsigned int type, const char *const type_sym,
	 const unsigned int flags, const struct strval32 *const known,
	 const size_t n, const char *const dflt)
{
	const struct nlmsghdr nlh = {
		.nlmsg_len = sizeof(nlh),
		.nlmsg_type = type,
		.nlmsg_flags = flags
	};

	jsonl_nl_sendto(fd, &nlh, sizeof(nlh));
	jsonl_nl_sendto_open(fd);
	jsonl_nlmsghdr_type_open(nlh.nlmsg_len);
	jsonl_const(type, true, type_sym, NULL);
	jsonl_field_close();
	jsonl_field_flags("nlmsg_flags", false, flags, known, n, dflt);
	jsonl_field_unsigned("nlmsg_seq", false, nlh.nlmsg_seq);
	jsonl_field_pid("nlmsg_pid", false, "tgid", nlh.nlmsg_pid);
	jsonl_struct_close();
	jsonl_nl_sendto_close();
}

static void
test_nlmsg_type(const int fd)
{
	test_hdr(fd, ARG_STR(CRYPTO_MSG_NEWALG), NLM_F_REQUEST,
		 nlm_f_request, ARRAY_SIZE(nlm_f_request), NULL);
}

static void
test_nlmsg_flags(const int fd)
{
	static const struct strval32 get_flags[] = {
		{ ARG_STR(NLM_F_REQUEST) },
		{ ARG_STR(NLM_F_DUMP) }
	};
	static const struct strval32 new_flags[] = {
		{ ARG_STR(NLM_F_ECHO) },
		{ ARG_STR(NLM_F_REPLACE) }
	};
	static const struct strval32 del_flags[] = {
		{ ARG_STR(NLM_F_ECHO) },
		{ ARG_STR(NLM_F_NONREC) }
	};

	test_hdr(fd, ARG_STR(CRYPTO_MSG_GETALG), NLM_F_REQUEST | NLM_F_DUMP,
		 get_flags, ARRAY_SIZE(get_flags), NULL);
	test_hdr(fd, ARG_STR(CRYPTO_MSG_NEWALG), NLM_F_ECHO | NLM_F_REPLACE,
		 new_flags, ARRAY_SIZE(new_flags), NULL);
	test_hdr(fd, ARG_STR(CRYPTO_MSG_DELALG), NLM_F_ECHO | NLM_F_NONREC,
		 del_flags, ARRAY_SIZE(del_flags), NULL);
	test_hdr(fd, ARG_STR(CRYPTO_MSG_UPDATEALG), NLM_F_REPLACE,
		 NULL, 0, "NLM_F_???");
}

static void
print_str_field(const char *const name, const char *const str,
		const size_t size, const bool first)
{
	const size_t len = strnlen(str, size);

	jsonl_field_open(name, first);
	if (len < size)
		jsonl_string_value(str, len, false);
	else
		jsonl_string_value(str, size - 1, true);
	jsonl_field_close();
}

static void
print_crypto_user_alg(const struct crypto_user_alg *const alg)
{
	jsonl_struct_open();
	print_str_field("cru_name", alg->cru_name,
			sizeof(alg->cru_name), true);
	print_str_field("cru_driver_name", alg->cru_driver_name,
			sizeof(alg->cru_driver_name), false);
	print_str_field("cru_module_name", alg->cru_module_name,
			sizeof(alg->cru_module_name), false);
	jsonl_field_hex("cru_type", false, alg->cru_type);
	jsonl_field_hex("cru_mask", false, alg->cru_mask);
	jsonl_field_unsigned("cru_refcnt", false, alg->cru_refcnt);
	jsonl_field_hex("cru_flags", false, alg->cru_flags);
	jsonl_struct_close();
}

static void
test_crypto_msg_newalg(const int fd)
{
	struct crypto_user_alg alg = {
		.cru_name = "abcd",
		.cru_driver_name = "efgh",
		.cru_module_name = "dcba",
		.cru_type = 0xabcdfabc,
		.cru_mask = 0xfedabacd,
		.cru_refcnt = 0xbcacfacd,
		.cru_flags = 0xefacdbad
	};
	void *const nlh0 = midtail_alloc(NLMSG_HDRLEN, sizeof(alg));

	JSONL_NETLINK_OBJECT_EX_(fd, nlh0,
				 CRYPTO_MSG_NEWALG, "CRYPTO_MSG_NEWALG", NULL,
				 NLM_F_REQUEST, nlm_f_request,
				 ARRAY_SIZE(nlm_f_request),
				 alg, jsonl_nl_string,
				 print_crypto_user_alg(&alg));

	fill_memory_ex(alg.cru_name, sizeof(alg.cru_name), '0', 10);
	fill_memory_ex(alg.cru_driver_name, sizeof(alg.cru_driver_name),
		       'a', 'z' - 'a' + 1);
	fill_memory_ex(alg.cru_module_name, sizeof(alg.cru_module_name),
		       'A', 'Z' - 'A' + 1);

	JSONL_NETLINK_OBJECT_EX_(fd, nlh0,
				 CRYPTO_MSG_NEWALG, "CRYPTO_MSG_NEWALG", NULL,
				 NLM_F_REQUEST, nlm_f_request,
				 ARRAY_SIZE(nlm_f_request),
				 alg, jsonl_nl_string,
				 print_crypto_user_alg(&alg));
}

static void
test_crypto_msg_unspec(const int fd)
{
	static const char data[] = "abcd";
	void *const nlh0 = midtail_alloc(NLMSG_HDRLEN, sizeof(data) - 1);

	JSONL_NETLINK_(fd, nlh0,
		       0xffff, NULL, "CRYPTO_MSG_???",
		       NLM_F_REQUEST, nlm_f_request, ARRAY_SIZE(nlm_f_request),
		       sizeof(data) - 1, data, sizeof(data) - 1,
		       jsonl_bytes_value(data, sizeof(data) - 1, false));
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	int fd = create_nl_socket(NETLINK_CRYPTO);

	test_nlmsg_type(fd);
	test_nlmsg_flags(fd);
	test_crypto_msg_newalg(fd);
	test_crypto_msg_unspec(fd);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
