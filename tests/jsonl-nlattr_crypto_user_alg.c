/*
 * Check JSONL output of CRYPTOCFGA_* netlink attributes of crypto_user_alg.
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

# define CRYPTOCFGA_REPORT_LARVAL 2

static const struct crypto_user_alg alg = {
	.cru_name = "abcd",
	.cru_driver_name = "efgh",
	.cru_module_name = "ijkl",
};

static void
init_crypto_user_alg(struct nlmsghdr *const nlh, const unsigned int msg_len)
{
	SET_STRUCT(struct nlmsghdr, nlh,
		.nlmsg_len = msg_len,
		.nlmsg_type = CRYPTO_MSG_GETALG,
		.nlmsg_flags = NLM_F_DUMP
	);

	memcpy(NLMSG_DATA(nlh), &alg, sizeof(alg));
}

static void
print_str_field(const char *const name, const char *const str, const bool first)
{
	jsonl_field_open(name, first);
	jsonl_string_value(str, strlen(str), false);
	jsonl_field_close();
}

static void
print_u32_field(const char *const name, const uint32_t val)
{
	jsonl_field_open(name, false);
	jsonl_unsigned(val);
	jsonl_field_close();
}

static void
print_x32_field(const char *const name, const uint32_t val)
{
	jsonl_field_open(name, false);
	jsonl_hex(val);
	jsonl_field_close();
}

static void
print_crypto_user_alg(const unsigned int msg_len)
{
	static const struct strval32 nlmsg_flags[] = {
		{ ARG_STR(NLM_F_DUMP) }
	};

	printf("{");
	jsonl_nlmsghdr_vals(msg_len, ARG_STR(CRYPTO_MSG_GETALG), NULL,
			    NLM_F_DUMP, nlmsg_flags, ARRAY_SIZE(nlmsg_flags));
	printf("}, {");
	jsonl_struct_open();
	print_str_field("cru_name", alg.cru_name, true);
	print_str_field("cru_driver_name", alg.cru_driver_name, false);
	print_str_field("cru_module_name", alg.cru_module_name, false);
	print_x32_field("cru_type", alg.cru_type);
	print_x32_field("cru_mask", alg.cru_mask);
	print_u32_field("cru_refcnt", alg.cru_refcnt);
	print_x32_field("cru_flags", alg.cru_flags);
	jsonl_struct_close();
	printf("}");
}

static void
jsonl_nl_string(const void *const p, const size_t len)
{
	jsonl_string_value(p, len, false);
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	const int fd = create_nl_socket(NETLINK_CRYPTO);
	const unsigned int hdrlen = sizeof(struct crypto_user_alg);
	/*
	 * There are also other structures, but they are not bigger than
	 * DEFAULT_STRLEN so far.
	 */
	void *const nlh0 = midtail_alloc(NLMSG_SPACE(hdrlen),
					 NLA_HDRLEN + DEFAULT_STRLEN);

	static char pattern[4096];
	fill_memory_ex(pattern, sizeof(pattern), 'a', 'z' - 'a' + 1);

	char *const str = tail_alloc(DEFAULT_STRLEN);
	fill_memory_ex(str, DEFAULT_STRLEN, '0', 10);
	JSONL_NLATTR(fd, nlh0, hdrlen,
		     init_crypto_user_alg, print_crypto_user_alg,
		     CRYPTOCFGA_REPORT_LARVAL,
		     DEFAULT_STRLEN, str, DEFAULT_STRLEN,
		     jsonl_struct_open();
		     jsonl_field_open("type", true);
		     jsonl_string_value(str, DEFAULT_STRLEN, true);
		     jsonl_field_close();
		     jsonl_struct_close());
	str[DEFAULT_STRLEN - 1] = '\0';
	JSONL_NLATTR(fd, nlh0, hdrlen,
		     init_crypto_user_alg, print_crypto_user_alg,
		     CRYPTOCFGA_REPORT_LARVAL,
		     DEFAULT_STRLEN, str, DEFAULT_STRLEN,
		     jsonl_struct_open();
		     print_str_field("type", str, true);
		     jsonl_struct_close());

	static const struct crypto_report_hash rhash = {
		.type = "efgh",
		.blocksize = 0xabcdefdc,
		.digestsize = 0xfebcdacd
	};
	JSONL_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
			       init_crypto_user_alg, print_crypto_user_alg,
			       CRYPTOCFGA_REPORT_HASH,
			       pattern, rhash, sizeof(rhash),
			       jsonl_nl_string,
			       jsonl_struct_open();
			       print_str_field("type", rhash.type, true);
			       print_u32_field("blocksize", rhash.blocksize);
			       print_u32_field("digestsize", rhash.digestsize);
			       jsonl_struct_close());

	static const struct crypto_report_blkcipher rblkcipher = {
		.type = "abcd",
		.geniv = "efgh",
		.blocksize = 0xabcdefac,
		.min_keysize = 0xfeadbcda,
		.max_keysize = 0xbdacdeac,
		.ivsize = 0xefacbdac
	};
	JSONL_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
			       init_crypto_user_alg, print_crypto_user_alg,
			       CRYPTOCFGA_REPORT_BLKCIPHER,
			       pattern, rblkcipher, sizeof(rblkcipher),
			       jsonl_nl_string,
			       jsonl_struct_open();
			       print_str_field("type", rblkcipher.type, true);
			       print_str_field("geniv", rblkcipher.geniv,
					       false);
			       print_u32_field("blocksize",
					       rblkcipher.blocksize);
			       print_u32_field("min_keysize",
					       rblkcipher.min_keysize);
			       print_u32_field("max_keysize",
					       rblkcipher.max_keysize);
			       print_u32_field("ivsize", rblkcipher.ivsize);
			       jsonl_struct_close());

	static const struct crypto_report_aead raead = {
		.type = "abcd",
		.geniv = "efgh",
		.blocksize = 0xbaefdbac,
		.maxauthsize = 0xfdbdbcda,
		.ivsize = 0xacbefdac
	};
	JSONL_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
			       init_crypto_user_alg, print_crypto_user_alg,
			       CRYPTOCFGA_REPORT_AEAD,
			       pattern, raead, sizeof(raead),
			       jsonl_nl_string,
			       jsonl_struct_open();
			       print_str_field("type", raead.type, true);
			       print_str_field("geniv", raead.geniv, false);
			       print_u32_field("blocksize", raead.blocksize);
			       print_u32_field("maxauthsize",
					       raead.maxauthsize);
			       print_u32_field("ivsize", raead.ivsize);
			       jsonl_struct_close());

	static const struct crypto_report_rng rrng = {
		.type = "abcd",
		.seedsize = 0xabcdefac
	};
	JSONL_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
			       init_crypto_user_alg, print_crypto_user_alg,
			       CRYPTOCFGA_REPORT_RNG,
			       pattern, rrng, sizeof(rrng), jsonl_nl_string,
			       jsonl_struct_open();
			       print_str_field("type", rrng.type, true);
			       print_u32_field("seedsize", rrng.seedsize);
			       jsonl_struct_close());

	static const struct crypto_report_cipher rcipher = {
		.type = "abcd",
		.blocksize = 0xabcdefac,
		.min_keysize = 0xfeadbcda,
		.max_keysize = 0xbdacdeac,
	};
	JSONL_NLATTR_OBJECT_EX(fd, nlh0, hdrlen,
			       init_crypto_user_alg, print_crypto_user_alg,
			       CRYPTOCFGA_REPORT_CIPHER,
			       pattern, rcipher, sizeof(rcipher),
			       jsonl_nl_string,
			       jsonl_struct_open();
			       print_str_field("type", rcipher.type, true);
			       print_u32_field("blocksize", rcipher.blocksize);
			       print_u32_field("min_keysize",
					       rcipher.min_keysize);
			       print_u32_field("max_keysize",
					       rcipher.max_keysize);
			       jsonl_struct_close());

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
