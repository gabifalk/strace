/*
 * Check JSONL output of the keyctl syscall.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"
#include "scno.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <linux/keyctl.h>

#include "xlat.h"
#include "xlat/key_perms.h"
#include "xlat/keyctl_caps0.h"
#include "xlat/keyctl_caps1.h"
#include "xlat/keyctl_move_flags.h"

/* From ioctl_dm.c */
#define STR32 "AbCdEfGhIjKlMnOpQrStUvWxYz012345"

enum { IOV_SIZE = 11, IOV_READABLE = 4 };

static const size_t limit = 10;

static bool merged;
static pid_t pid;
static struct iovec *key_iov;

struct arg {
	const char *name;
	kernel_ulong_t val;
	void (*print)(const struct arg *, long rc);
	const char *sym;
	const char *sym_pattern;
	const void *data;
	size_t size;
	bool flag;
	bool exit;
};

/* A decimal const value. */
static void
jsonl_const_d(int val, const char *sym, const char *sym_pattern)
{
	printf("\"type\": \"const\", \"raw\": \"%d\"", val);
	if (sym)
		printf(", \"sym\": \"%s\"", sym);
	else if (sym_pattern)
		printf(", \"sym_pattern\": \"%s\"", sym_pattern);
}

/*
 * A flags value decoded using an xlat table: an entry matches
 * when all its bits are still set after the preceding matches.
 */
static void
jsonl_flags_xlat(const struct xlat *xlat, uint64_t raw, const char *dflt)
{
	uint64_t rem = raw;
	bool first = true;

	jsonl_flags_open(raw);
	for (size_t i = 0; i < xlat->size; ++i) {
		const uint64_t val = xlat->data[i].val;

		if (!xlat->data[i].str ||
		    (val ? (rem & val) != val : raw != 0))
			continue;
		jsonl_flags_const(val, xlat->data[i].str, first);
		rem &= ~val;
		first = false;
	}
	if (rem)
		jsonl_flags_remainder(rem, first ? dflt : NULL, first);
	jsonl_flags_close();
}

/* The first len bytes of str, cut at the string length limit. */
static void
jsonl_string_limit(const char *str, size_t len)
{
	jsonl_string_value(str, MIN(len, limit), len > limit);
}

static void
jsonl_spare(const uint32_t *spare, size_t n)
{
	jsonl_field_open("__spare", false);
	jsonl_array_open();
	for (size_t i = 0; i < n; ++i) {
		printf("%s{", i ? ", " : "");
		jsonl_hex(spare[i]);
		printf("}");
	}
	jsonl_array_close();
	jsonl_field_close();
}

static void
p_hex(const struct arg *a, long rc)
{
	jsonl_hex(a->val);
}

static void
p_const_d(const struct arg *a, long rc)
{
	jsonl_const_d(a->val, a->sym, a->sym_pattern);
}

static void
p_dec(const struct arg *a, long rc)
{
	jsonl_decimal((int) a->val);
}

static void
p_unsigned(const struct arg *a, long rc)
{
	jsonl_unsigned(a->val);
}

static void
p_u32(const struct arg *a, long rc)
{
	jsonl_unsigned((uint32_t) a->val);
}

/* A user or a group ID, the name of the type is in sym. */
static void
p_id(const struct arg *a, long rc)
{
	if ((uint32_t) a->val == -1U)
		printf("\"type\": \"%s\", \"raw\": \"-1\"", a->sym);
	else
		printf("\"type\": \"%s\", \"raw\": \"%u\"",
		       a->sym, (uint32_t) a->val);
}

static void
p_addr(const struct arg *a, long rc)
{
	jsonl_addr((const void *) (uintptr_t) a->val);
}

static void
p_str(const struct arg *a, long rc)
{
	const char *str = (const char *) (uintptr_t) a->val;

	jsonl_string_limit(str, strnlen(str, limit + 1));
}

static void
p_mem(const struct arg *a, long rc)
{
	jsonl_string_limit((const char *) (uintptr_t) a->val, a->size);
}

/*
 * A buffer of the given size filled by the kernel with rc bytes
 * of data that is NUL-terminated if flag is set.
 */
static void
p_outbuf(const struct arg *a, long rc)
{
	const char *str = (const char *) (uintptr_t) a->val;

	if (rc < 0 || !str) {
		jsonl_addr(str);
		return;
	}

	size_t print_size = MIN((size_t) rc, a->size);
	size_t limited_size = MIN(print_size, limit);
	size_t len = strnlen(str, limited_size);

	if (a->flag && len < limited_size)
		jsonl_string_value(str, len, false);
	else
		jsonl_string_value(str, limited_size, print_size > limit);
}

/* An error number is a plain number unless its name is in sym. */
static void
p_error(const struct arg *a, long rc)
{
	if (a->sym)
		printf("\"type\": \"errno\", \"raw\": \"%u\", \"sym\": \"%s\"",
		       (uint32_t) a->val, a->sym);
	else
		p_u32(a, rc);
}

static void
p_flags(const struct arg *a, long rc)
{
	jsonl_flags_xlat(a->data, (uint32_t) a->val, a->sym);
}

/* Only the first IOV_READABLE elements of key_iov point to strings. */
static void
p_iov(const struct arg *a, long rc)
{
	const struct iovec *iov = (const struct iovec *) (uintptr_t) a->val;
	size_t i;

	jsonl_array_open();
	for (i = 0; i < a->size && i < limit; ++i) {
		printf("%s{", i ? ", " : "");
		jsonl_struct_open();
		jsonl_field_open("iov_base", true);
		if (&iov[i] < &key_iov[IOV_READABLE])
			jsonl_string_limit(iov[i].iov_base, iov[i].iov_len);
		else
			jsonl_addr(iov[i].iov_base);
		jsonl_field_close();
		jsonl_field_open("iov_len", false);
		jsonl_unsigned(iov[i].iov_len);
		jsonl_field_close();
		jsonl_struct_close();
		printf("}");
	}
	jsonl_array_close();
	if (i < a->size)
		jsonl_truncated();
}

/* The name of the private key is in sym. */
static void
p_dh_params(const struct arg *a, long rc)
{
	const struct keyctl_dh_params *p =
		(const struct keyctl_dh_params *) (uintptr_t) a->val;

	jsonl_struct_open();
	jsonl_field_open("private", true);
	jsonl_const_d(p->private, a->sym, NULL);
	jsonl_field_close();
	jsonl_field_open("prime", false);
	jsonl_const_d(p->prime, NULL, NULL);
	jsonl_field_close();
	jsonl_field_open("base", false);
	jsonl_const_d(p->base, NULL, NULL);
	jsonl_field_close();
	jsonl_struct_close();
}

enum {
	KDF_DEREF_HASH = 1,
	KDF_DEREF_OTHERINFO = 2,
	KDF_PRINT_SPARE = 4
};

/* The size is a combination of KDF_* flags. */
static void
p_kdf_params(const struct arg *a, long rc)
{
	const struct keyctl_kdf_params *kdf =
		(const struct keyctl_kdf_params *) (uintptr_t) a->val;

	jsonl_struct_open();

	jsonl_field_open("hashname", true);
	if (kdf->hashname && (a->size & KDF_DEREF_HASH))
		jsonl_string_limit(kdf->hashname,
				   strnlen(kdf->hashname, limit + 1));
	else
		jsonl_addr(kdf->hashname);
	jsonl_field_close();

	jsonl_field_open("otherinfo", false);
	if (kdf->otherinfo && (a->size & KDF_DEREF_OTHERINFO))
		jsonl_string_limit(kdf->otherinfo, kdf->otherinfolen);
	else
		jsonl_addr(kdf->otherinfo);
	jsonl_field_close();

	jsonl_field_open("otherinfolen", false);
	jsonl_unsigned(kdf->otherinfolen);
	jsonl_field_close();

	if (a->size & KDF_PRINT_SPARE)
		jsonl_spare(kdf->__spare, ARRAY_SIZE(kdf->__spare));

	jsonl_struct_close();
}

/*
 * The name of the key is in sym, flag is set when the operation
 * has an output buffer, size is set when __spare is printed.
 */
static void
p_pkey_params(const struct arg *a, long rc)
{
	const struct keyctl_pkey_params *p =
		(const struct keyctl_pkey_params *) (uintptr_t) a->val;

	jsonl_struct_open();
	jsonl_field_open("key_id", true);
	jsonl_const_d(p->key_id, a->sym, NULL);
	jsonl_field_close();
	jsonl_field_open("in_len", false);
	jsonl_unsigned(p->in_len);
	jsonl_field_close();
	jsonl_field_open(a->flag ? "out_len" : "in2_len", false);
	jsonl_unsigned(p->out_len);
	jsonl_field_close();
	if (a->size)
		jsonl_spare(p->__spare, ARRAY_SIZE(p->__spare));
	jsonl_struct_close();
}

/* A buffer of the given size filled by the kernel with rc bytes. */
static void
p_caps(const struct arg *a, long rc)
{
	const unsigned char *caps =
		(const unsigned char *) (uintptr_t) a->val;

	if (rc < 0 || !caps) {
		jsonl_addr(caps);
		return;
	}

	size_t n = MIN((size_t) rc, a->size);

	jsonl_array_open();
	if (n >= 1) {
		printf("{");
		jsonl_flags_xlat(keyctl_caps0, caps[0], "KEYCTL_CAPS0_???");
		printf("}");
	}
	if (n >= 2) {
		printf(", {");
		jsonl_flags_xlat(keyctl_caps1, caps[1], "KEYCTL_CAPS1_???");
		printf("}");
	}
	jsonl_array_close();
}

#define ARG(print_, name_, val_, ...) \
	((struct arg) { .name = (name_), .val = (kernel_ulong_t) (val_), \
			.print = (print_), __VA_ARGS__ })

#define A_HEX(name_, val_)	ARG(p_hex, name_, val_)
#define A_DEC(name_, val_)	ARG(p_dec, name_, val_)
#define A_UNSIGNED(name_, val_)	ARG(p_unsigned, name_, val_)
#define A_U32(name_, val_)	ARG(p_u32, name_, val_)
#define A_UID(val_)		ARG(p_id, "uid", (uint32_t) (val_), .sym = "uid")
#define A_GID(val_)		ARG(p_id, "gid", (uint32_t) (val_), .sym = "gid")
#define A_ADDR(name_, ptr_)	ARG(p_addr, name_, (uintptr_t) (ptr_))
#define A_STR(name_, ptr_)	ARG(p_str, name_, (uintptr_t) (ptr_))
#define A_MEM(name_, ptr_, size_) \
	ARG(p_mem, name_, (uintptr_t) (ptr_), .size = (size_))
#define A_FLAGS(name_, val_, xlat_, dflt_) \
	ARG(p_flags, name_, val_, .data = (xlat_), .sym = (dflt_))

/* A key serial number without a name. */
#define A_KEY(name_, val_)	ARG(p_const_d, name_, val_)
/* A special key: A_KEY_SPEC(name, ARG_STR(KEY_SPEC_...)). */
#define A_KEY_SPEC(name_, ...)	A_KEY_SPEC_(name_, __VA_ARGS__)
#define A_KEY_SPEC_(name_, val_, sym_) \
	ARG(p_const_d, name_, (int) (val_), .sym = (sym_))

/* Output arguments are printed on exiting. */
#define X_ADDR(name_, ptr_) \
	ARG(p_addr, name_, (uintptr_t) (ptr_), .exit = true)
#define X_UNSIGNED(name_, val_) \
	ARG(p_unsigned, name_, val_, .exit = true)
#define X_BUF(name_, ptr_, size_, nul_terminated_) \
	ARG(p_outbuf, name_, (uintptr_t) (ptr_), .size = (size_), \
	    .flag = (nul_terminated_), .exit = true)

static void
arg_print(unsigned int index, const struct arg *a, long rc, bool first)
{
	const char *side = merged ? (a->exit ? "exit" : "entry") : NULL;

	if (!first)
		printf(", ");
	jsonl_arg_open(index, a->name, side);
	a->print(a, rc);
	jsonl_arg_close(side);
}

/* The operation is followed by cnt arguments of type struct arg. */
static long
do_keyctl(kernel_ulong_t cmd, const char *cmd_str, unsigned int cnt, ...)
{
	kernel_ulong_t args[4] = {
		(kernel_ulong_t) 0xdeadfee1badc0de5ULL,
		(kernel_ulong_t) 0xdeadfee2badc0de6ULL,
		(kernel_ulong_t) 0xdeadfee3badc0de7ULL,
		(kernel_ulong_t) 0xdeadfee4badc0de8ULL,
	};
	struct arg a[4];
	va_list ap;

	va_start(ap, cnt);
	for (unsigned int i = 0; i < cnt; ++i) {
		a[i] = va_arg(ap, struct arg);
		args[i] = a[i].val;
	}
	va_end(ap);

	long rc = syscall(__NR_keyctl, cmd, args[0], args[1], args[2], args[3]);
	int saved_errno = errno;

	if (merged)
		jsonl_syscall_merged_open_pid(pid, "keyctl", __NR_keyctl);
	else
		jsonl_syscall_open_pid(pid, "keyctl", __NR_keyctl, true);

	jsonl_arg_open(0, "operation", merged ? "entry" : NULL);
	jsonl_const((unsigned int) cmd, true, cmd_str, "KEYCTL_???");
	jsonl_arg_close(merged ? "entry" : NULL);

	for (unsigned int i = 0; i < cnt; ++i) {
		if (!a[i].exit)
			arg_print(i + 1, &a[i], rc, false);
	}

	bool first = false;
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open_pid(pid, "keyctl", __NR_keyctl, false);
		first = true;
	}

	for (unsigned int i = 0; i < cnt; ++i) {
		if (a[i].exit) {
			arg_print(i + 1, &a[i], rc, first);
			first = false;
		}
	}

	printf("], ");
	errno = saved_errno;
	jsonl_return("unsigned", rc);
	printf("}\n");

	return rc;
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	pid = fork();
	if (pid < 0)
		perror_msg_and_fail("fork");
	if (pid) {
		int s;
		pid_t rc;
		while ((rc = waitpid(pid, &s, 0)) != pid) {
			if (rc < 0 && errno == EINTR)
				continue;
			perror_msg_and_fail("waitpid: %d", pid);
		}
		if (!WIFEXITED(s) || WEXITSTATUS(s))
			return WIFEXITED(s) ? WEXITSTATUS(s) : 1;
		jsonl_exit_event(0);
		return 0;
	}
	pid = getpid();

	static const char unterminated1[] = { '\1', '\2', '\3', '\4', '\5' };
	static const char unterminated2[] = { '\6', '\7', '\10', '\11', '\12' };
	static const char short_type_str[] = "shrt type";
	static const char short_desc_str[] = "shrt desc";
	static const char long_type_str[] = "overly long key type";
	static const char long_desc_str[] = "overly long key description";
	static const int32_t bogus_key1 = 0xdeadf00d;
	static const int32_t bogus_key2 = 0x1eefdead;
	static const kernel_ulong_t bogus_key3 =
		(kernel_ulong_t) 0xdec0ded1dec0ded2ULL;
	static const kernel_ulong_t bogus_size =
		(kernel_ulong_t) 0xfeedf157badc0dedULL;
	const char *const bogus_ptr1 =
		(const char *) (uintptr_t) 0xfffffacefffffeedULL;
	const char *const bogus_ptr2 =
		(const char *) (uintptr_t) 0xfffff00dfffff157ULL;

	static const struct keyctl_dh_params kcdhp_data = {
		{ .private = KEY_SPEC_GROUP_KEYRING },
		.prime = 1234567890,
		.base = 3141592653U
	};

	static const char long_hash_data[] = STR32 STR32 STR32 STR32 "xxx";
	static const char short_hash_data[] = "hmac(aes)";
	static const char otherinfo1_data[] = "\1\2 OH HAI THAR\255\0\1";
	static const char otherinfo2_data[] = "\1\2\n\255\0\1";

	char *bogus_str = tail_memdup(unterminated1, sizeof(unterminated1));
	char *bogus_desc = tail_memdup(unterminated2, sizeof(unterminated2));
	char *short_type = tail_memdup(short_type_str, sizeof(short_type_str));
	char *short_desc = tail_memdup(short_desc_str, sizeof(short_desc_str));
	char *long_type = tail_memdup(long_type_str, sizeof(long_type_str));
	char *long_desc = tail_memdup(long_desc_str, sizeof(long_desc_str));
	char *kcdhp = tail_memdup(&kcdhp_data, sizeof(kcdhp_data));
	char *kckdfp_long_hash = tail_memdup(long_hash_data,
					     sizeof(long_hash_data));
	char *kckdfp_short_hash = tail_memdup(short_hash_data,
					      sizeof(short_hash_data));
	char *kckdfp_otherinfo1 = tail_memdup(otherinfo1_data,
					      sizeof(otherinfo1_data));
	char *kckdfp_otherinfo2 = tail_memdup(otherinfo2_data,
					      sizeof(otherinfo2_data));
	TAIL_ALLOC_OBJECT_CONST_PTR(struct keyctl_kdf_params, kckdfp);
	char *bogus_buf1 = tail_alloc(9);
	char *bogus_buf2 = tail_alloc(256);

	key_iov = tail_alloc(sizeof(*key_iov) * IOV_SIZE);
	key_iov[0].iov_base = short_type;
	key_iov[0].iov_len = sizeof(short_type_str);
	key_iov[1].iov_base = long_type;
	key_iov[1].iov_len = sizeof(long_type_str);
	key_iov[2].iov_base = short_desc;
	key_iov[2].iov_len = sizeof(short_desc_str);
	key_iov[3].iov_base = long_desc;
	key_iov[3].iov_len = sizeof(long_desc_str);
	key_iov[4].iov_base = bogus_str;
	key_iov[4].iov_len = 32;

	for (size_t i = 5; i < IOV_SIZE; ++i) {
		key_iov[i].iov_base =
			(void *) (uintptr_t) (0xfffffacefffff00dULL +
			0x100000001ULL * i);
		key_iov[i].iov_len = (size_t) (0xcaffeeeddefaced7ULL +
			0x100000001ULL * i);
	}

	/* Invalid command */
	do_keyctl((kernel_ulong_t) 0xbadc0dedfacefeedULL, NULL, 4,
		  A_HEX("arg2", (kernel_ulong_t) 0xdeadfee1badc0de5ULL),
		  A_HEX("arg3", (kernel_ulong_t) 0xdeadfee2badc0de6ULL),
		  A_HEX("arg4", (kernel_ulong_t) 0xdeadfee3badc0de7ULL),
		  A_HEX("arg5", (kernel_ulong_t) 0xdeadfee4badc0de8ULL));

	/* KEYCTL_GET_KEYRING_ID */
	do_keyctl(ARG_STR(KEYCTL_GET_KEYRING_ID), 2,
		  A_KEY("key", bogus_key3),
		  A_DEC("create", (kernel_ulong_t) 0xbadc0dedffffffffULL));
	do_keyctl(ARG_STR(KEYCTL_GET_KEYRING_ID), 2,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_THREAD_KEYRING)),
		  A_DEC("create", 3141592653U));

	/* KEYCTL_JOIN_SESSION_KEYRING */
	do_keyctl(ARG_STR(KEYCTL_JOIN_SESSION_KEYRING), 1,
		  A_ADDR("name", NULL));
	do_keyctl(ARG_STR(KEYCTL_JOIN_SESSION_KEYRING), 1,
		  A_ADDR("name", bogus_ptr1));
	do_keyctl(ARG_STR(KEYCTL_JOIN_SESSION_KEYRING), 1,
		  A_ADDR("name", bogus_str));
	do_keyctl(ARG_STR(KEYCTL_JOIN_SESSION_KEYRING), 1,
		  A_STR("name", "bogus name"));
	do_keyctl(ARG_STR(KEYCTL_JOIN_SESSION_KEYRING), 1,
		  A_STR("name", "very long keyring name"));

	/* KEYCTL_UPDATE */
	do_keyctl(ARG_STR(KEYCTL_UPDATE), 3,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_REQUESTOR_KEYRING)),
		  A_ADDR("payload", NULL),
		  A_UNSIGNED("plen", 0));
	do_keyctl(ARG_STR(KEYCTL_UPDATE), 3,
		  A_KEY("key", bogus_key1),
		  A_ADDR("payload", bogus_ptr1),
		  A_UNSIGNED("plen", (kernel_ulong_t) 0xdeadfee4badc0de8ULL));
	do_keyctl(ARG_STR(KEYCTL_UPDATE), 3,
		  A_KEY("key", bogus_key2),
		  A_ADDR("payload", bogus_str),
		  A_UNSIGNED("plen", (kernel_ulong_t) 0xdeadfee4badc0de8ULL));
	do_keyctl(ARG_STR(KEYCTL_UPDATE), 3,
		  A_KEY("key", bogus_key3),
		  A_MEM("payload", short_desc, sizeof(short_desc_str) - 1),
		  A_UNSIGNED("plen", sizeof(short_desc_str) - 1));

	/* KEYCTL_REVOKE */
	do_keyctl(ARG_STR(KEYCTL_REVOKE), 1,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_GROUP_KEYRING)));
	do_keyctl(ARG_STR(KEYCTL_REVOKE), 1,
		  A_KEY("key", bogus_key1));

	/* KEYCTL_CHOWN */
	do_keyctl(ARG_STR(KEYCTL_CHOWN), 3,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_REQUESTOR_KEYRING)),
		  A_UID(-1), A_GID(-1));
	do_keyctl(ARG_STR(KEYCTL_CHOWN), 3,
		  A_KEY("key", bogus_key1),
		  A_UID(2718281828U), A_GID(3141592653U));

	/* KEYCTL_SETPERM */
	do_keyctl(ARG_STR(KEYCTL_SETPERM), 2,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_REQKEY_AUTH_KEY)),
		  A_FLAGS("perms", 0xffffffffU, key_perms, "KEY_???"));
	do_keyctl(ARG_STR(KEYCTL_SETPERM), 2,
		  A_KEY("key", bogus_key1),
		  A_FLAGS("perms", 0, key_perms, "KEY_???"));
	do_keyctl(ARG_STR(KEYCTL_SETPERM), 2,
		  A_KEY("key", bogus_key3),
		  A_FLAGS("perms", 0xc0c0c0c0, key_perms, "KEY_???"));

	/* KEYCTL_DESCRIBE */
	do_keyctl(ARG_STR(KEYCTL_DESCRIBE), 3,
		  A_KEY("key", bogus_key1),
		  X_ADDR("buffer", NULL),
		  X_UNSIGNED("buflen", bogus_size));
	do_keyctl(ARG_STR(KEYCTL_DESCRIBE), 3,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_THREAD_KEYRING)),
		  X_BUF("buffer", bogus_buf1, 9, true),
		  X_UNSIGNED("buflen", 9));
	do_keyctl(ARG_STR(KEYCTL_DESCRIBE), 3,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_THREAD_KEYRING)),
		  X_BUF("buffer", bogus_buf2, 256, true),
		  X_UNSIGNED("buflen", 256));
	do_keyctl(ARG_STR(KEYCTL_DESCRIBE), 3,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_THREAD_KEYRING)),
		  X_BUF("buffer", bogus_buf2, (size_t) -4, true),
		  X_UNSIGNED("buflen", (kernel_ulong_t) -4));

	/* KEYCTL_CLEAR */
	do_keyctl(ARG_STR(KEYCTL_CLEAR), 1,
		  A_KEY("key", bogus_key2));

	/* KEYCTL_LINK */
	do_keyctl(ARG_STR(KEYCTL_LINK), 2,
		  A_KEY("key", bogus_key1),
		  A_KEY_SPEC("keyring", ARG_STR(KEY_SPEC_GROUP_KEYRING)));
	do_keyctl(ARG_STR(KEYCTL_LINK), 2,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_REQUESTOR_KEYRING)),
		  A_KEY("keyring", bogus_key3));

	/* KEYCTL_UNLINK */
	do_keyctl(ARG_STR(KEYCTL_UNLINK), 2,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_REQUESTOR_KEYRING)),
		  A_KEY("keyring", bogus_key2));

	/* KEYCTL_SEARCH */
	do_keyctl(ARG_STR(KEYCTL_SEARCH), 4,
		  A_KEY_SPEC("keyring", ARG_STR(KEY_SPEC_REQUESTOR_KEYRING)),
		  A_ADDR("type", NULL),
		  A_ADDR("description", NULL),
		  A_KEY("dest_keyring", 0));
	do_keyctl(ARG_STR(KEYCTL_SEARCH), 4,
		  A_KEY("keyring", bogus_key1),
		  A_ADDR("type", bogus_ptr1),
		  A_ADDR("description", bogus_ptr2),
		  A_KEY_SPEC("dest_keyring",
			     ARG_STR(KEY_SPEC_USER_SESSION_KEYRING)));
	do_keyctl(ARG_STR(KEYCTL_SEARCH), 4,
		  A_KEY("keyring", bogus_key2),
		  A_ADDR("type", bogus_str),
		  A_ADDR("description", bogus_desc),
		  A_KEY("dest_keyring", bogus_key1));
	do_keyctl(ARG_STR(KEYCTL_SEARCH), 4,
		  A_KEY("keyring", bogus_key3),
		  A_STR("type", short_type),
		  A_STR("description", short_desc),
		  A_KEY("dest_keyring", bogus_key2));
	do_keyctl(ARG_STR(KEYCTL_SEARCH), 4,
		  A_KEY("keyring", 0),
		  A_STR("type", long_type),
		  A_STR("description", long_desc),
		  A_KEY("dest_keyring", bogus_key3));

	/* KEYCTL_RESTRICT_KEYRING */
	do_keyctl(ARG_STR(KEYCTL_RESTRICT_KEYRING), 3,
		  A_KEY_SPEC("keyring", ARG_STR(KEY_SPEC_REQUESTOR_KEYRING)),
		  A_ADDR("type", NULL),
		  A_ADDR("restriction", NULL));
	do_keyctl(ARG_STR(KEYCTL_RESTRICT_KEYRING), 3,
		  A_KEY("keyring", bogus_key2),
		  A_ADDR("type", bogus_str),
		  A_ADDR("restriction", bogus_desc));
	do_keyctl(ARG_STR(KEYCTL_RESTRICT_KEYRING), 3,
		  A_KEY("keyring", 0),
		  A_STR("type", long_type),
		  A_STR("restriction", short_desc));

	/* KEYCTL_READ */
	bogus_buf1[0] = '\377';
	bogus_buf2[0] = '\377';

	do_keyctl(ARG_STR(KEYCTL_READ), 3,
		  A_KEY("key", bogus_key1),
		  X_ADDR("buffer", NULL),
		  X_UNSIGNED("buflen", bogus_size));
	do_keyctl(ARG_STR(KEYCTL_READ), 3,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_THREAD_KEYRING)),
		  X_BUF("buffer", bogus_buf1, 9, false),
		  X_UNSIGNED("buflen", 9));
	do_keyctl(ARG_STR(KEYCTL_READ), 3,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_THREAD_KEYRING)),
		  X_BUF("buffer", bogus_buf2, 256, false),
		  X_UNSIGNED("buflen", 256));
	do_keyctl(ARG_STR(KEYCTL_READ), 3,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_THREAD_KEYRING)),
		  X_BUF("buffer", bogus_buf2, (size_t) -4, false),
		  X_UNSIGNED("buflen", (kernel_ulong_t) -4));

	/* KEYCTL_INSTANTIATE */
	do_keyctl(ARG_STR(KEYCTL_INSTANTIATE), 4,
		  A_KEY("key", 0),
		  A_ADDR("payload", NULL),
		  A_UNSIGNED("plen", bogus_size),
		  A_KEY("keyring", 0));
	do_keyctl(ARG_STR(KEYCTL_INSTANTIATE), 4,
		  A_KEY("key", bogus_key2),
		  A_ADDR("payload", bogus_str),
		  A_UNSIGNED("plen", 32),
		  A_KEY("keyring", bogus_key2));
	do_keyctl(ARG_STR(KEYCTL_INSTANTIATE), 4,
		  A_KEY("key", bogus_key3),
		  A_MEM("payload", short_desc, sizeof(short_type_str) - 1),
		  A_UNSIGNED("plen", sizeof(short_type_str) - 1),
		  A_KEY("keyring", bogus_key3));
	do_keyctl(ARG_STR(KEYCTL_INSTANTIATE), 4,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_GROUP_KEYRING)),
		  A_MEM("payload", long_desc, sizeof(long_type_str)),
		  A_UNSIGNED("plen", sizeof(long_type_str)),
		  A_KEY_SPEC("keyring", ARG_STR(KEY_SPEC_GROUP_KEYRING)));

	/* KEYCTL_NEGATE */
	do_keyctl(ARG_STR(KEYCTL_NEGATE), 3,
		  A_KEY("key", bogus_key1),
		  A_UNSIGNED("timeout", 3141592653U),
		  A_KEY("keyring", bogus_key1));
	do_keyctl(ARG_STR(KEYCTL_NEGATE), 3,
		  A_KEY("key", bogus_key3),
		  A_U32("timeout", bogus_size),
		  A_KEY("keyring", bogus_key3));

	/* KEYCTL_SET_REQKEY_KEYRING */
	do_keyctl(ARG_STR(KEYCTL_SET_REQKEY_KEYRING), 1,
		  ARG(p_const_d, "reqkey_defl", KEY_REQKEY_DEFL_NO_CHANGE,
		      .sym = "KEY_REQKEY_DEFL_NO_CHANGE"));
	do_keyctl(ARG_STR(KEYCTL_SET_REQKEY_KEYRING), 1,
		  ARG(p_const_d, "reqkey_defl", bogus_size,
		      .sym_pattern = "KEY_REQKEY_DEFL_???"));

	/* KEYCTL_SET_TIMEOUT */
	do_keyctl(ARG_STR(KEYCTL_SET_TIMEOUT), 2,
		  A_KEY("key", bogus_key1),
		  A_UNSIGNED("timeout", 3141592653U));

	/* KEYCTL_ASSUME_AUTHORITY */
	do_keyctl(ARG_STR(KEYCTL_ASSUME_AUTHORITY), 1,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_GROUP_KEYRING)));

	/* KEYCTL_GET_SECURITY */
	do_keyctl(ARG_STR(KEYCTL_GET_SECURITY), 3,
		  A_KEY("key", bogus_key1),
		  X_ADDR("buffer", NULL),
		  X_UNSIGNED("buflen", 0xbadc0dedU));
	do_keyctl(ARG_STR(KEYCTL_GET_SECURITY), 3,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_THREAD_KEYRING)),
		  X_BUF("buffer", bogus_buf1, 9, true),
		  X_UNSIGNED("buflen", 9));
	do_keyctl(ARG_STR(KEYCTL_GET_SECURITY), 3,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_THREAD_KEYRING)),
		  X_BUF("buffer", bogus_buf2, 256, true),
		  X_UNSIGNED("buflen", 256));
	do_keyctl(ARG_STR(KEYCTL_GET_SECURITY), 3,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_THREAD_KEYRING)),
		  X_BUF("buffer", bogus_buf2, (size_t) -4, true),
		  X_UNSIGNED("buflen", (kernel_ulong_t) -4));

	/* KEYCTL_SESSION_TO_PARENT */
	do_keyctl(ARG_STR(KEYCTL_SESSION_TO_PARENT), 0);

	/* KEYCTL_REJECT */
	do_keyctl(ARG_STR(KEYCTL_REJECT), 4,
		  A_KEY("key", bogus_key1),
		  A_UNSIGNED("timeout", 3141592653U),
		  ARG(p_error, "error", 2718281828U),
		  A_KEY("keyring", bogus_key1));
	do_keyctl(ARG_STR(KEYCTL_REJECT), 4,
		  A_KEY("key", bogus_key3),
		  A_U32("timeout", bogus_size),
		  ARG(p_error, "error", ENODEV, .sym = "ENODEV"),
		  A_KEY("keyring", bogus_key3));

	/* KEYCTL_INSTANTIATE_IOV */
	do_keyctl(ARG_STR(KEYCTL_INSTANTIATE_IOV), 4,
		  A_KEY("key", 0),
		  A_ADDR("payload", NULL),
		  A_UNSIGNED("plen", bogus_size),
		  A_KEY("keyring", 0));
	do_keyctl(ARG_STR(KEYCTL_INSTANTIATE_IOV), 4,
		  A_KEY("key", bogus_key2),
		  A_ADDR("payload", key_iov + IOV_SIZE),
		  A_UNSIGNED("plen", 32),
		  A_KEY("keyring", bogus_key2));
	do_keyctl(ARG_STR(KEYCTL_INSTANTIATE_IOV), 4,
		  A_KEY("key", bogus_key3),
		  ARG(p_iov, "payload", (uintptr_t) (key_iov + IOV_SIZE - 4),
		      .size = 4),
		  A_UNSIGNED("plen", 4),
		  A_KEY("keyring", bogus_key3));
	do_keyctl(ARG_STR(KEYCTL_INSTANTIATE_IOV), 4,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_GROUP_KEYRING)),
		  ARG(p_iov, "payload", (uintptr_t) key_iov,
		      .size = IOV_SIZE),
		  A_UNSIGNED("plen", IOV_SIZE),
		  A_KEY_SPEC("keyring", ARG_STR(KEY_SPEC_GROUP_KEYRING)));

	/* KEYCTL_INVALIDATE */
	do_keyctl(ARG_STR(KEYCTL_INVALIDATE), 1,
		  A_KEY("key", bogus_key2));

	/* KEYCTL_GET_PERSISTENT */
	do_keyctl(ARG_STR(KEYCTL_GET_PERSISTENT), 2,
		  A_UID(-1),
		  A_KEY_SPEC("keyring", ARG_STR(KEY_SPEC_GROUP_KEYRING)));
	do_keyctl(ARG_STR(KEYCTL_GET_PERSISTENT), 2,
		  A_UID(2718281828U),
		  A_KEY("keyring", bogus_key1));

	/* KEYCTL_DH_COMPUTE */
	bogus_buf1[0] = '\377';
	bogus_buf2[0] = '\377';

#define A_DH_PARAMS \
	ARG(p_dh_params, "dh_params", (uintptr_t) kcdhp, \
	    .sym = "KEY_SPEC_GROUP_KEYRING")

	do_keyctl(ARG_STR(KEYCTL_DH_COMPUTE), 4,
		  A_ADDR("dh_params", NULL),
		  X_ADDR("buffer", NULL),
		  X_UNSIGNED("buflen", bogus_size),
		  X_ADDR("kdf_params", NULL));
	do_keyctl(ARG_STR(KEYCTL_DH_COMPUTE), 4,
		  A_ADDR("dh_params", kcdhp + 1),
		  X_ADDR("buffer", (uintptr_t) 0xfffff157ffffdeadULL),
		  X_UNSIGNED("buflen", bogus_size),
		  X_ADDR("kdf_params", NULL));
	do_keyctl(ARG_STR(KEYCTL_DH_COMPUTE), 4,
		  A_DH_PARAMS,
		  X_BUF("buffer", bogus_buf1, 9, false),
		  X_UNSIGNED("buflen", 9),
		  X_ADDR("kdf_params", NULL));
	do_keyctl(ARG_STR(KEYCTL_DH_COMPUTE), 4,
		  A_DH_PARAMS,
		  X_BUF("buffer", bogus_buf2, (size_t) -1, false),
		  X_UNSIGNED("buflen", (kernel_ulong_t) -1),
		  X_ADDR("kdf_params", (char *) kckdfp + 1));

	/* KEYCTL_DH_COMPUTE + KDF */
	static const struct keyctl_kdf_params kckdfp_data[] = {
		[0] = { NULL, NULL, 0, { 0 } },
		[1] = { NULL, NULL, 0xbadc0dedU, { [7] = 0xdeadfeedU } },
		[2] = { NULL, NULL, 0, { 0 } },
		[3] = { NULL, NULL, 0, { 1 } },
		[4] = { NULL, NULL, sizeof(otherinfo1_data),
			{ 0, 0xfacebeef, 0, 0xba5e1ead } },
		[5] = { NULL, NULL, sizeof(otherinfo2_data), { 0 } },
	};

	for (size_t i = 0; i < ARRAY_SIZE(kckdfp_data); ++i) {
		unsigned int flags = KDF_DEREF_HASH | KDF_DEREF_OTHERINFO;

		memcpy(kckdfp, kckdfp_data + i, sizeof(kckdfp_data[i]));

		switch (i) {
		case 1:
			/* The hash name is not readable. */
			flags = KDF_DEREF_OTHERINFO | KDF_PRINT_SPARE;
			kckdfp->hashname =
				kckdfp_short_hash + sizeof(short_hash_data);
			break;
		case 2:
			/* The other info is empty and is not readable. */
			flags = KDF_DEREF_HASH;
			kckdfp->hashname = kckdfp_long_hash;
			kckdfp->otherinfo =
				kckdfp_otherinfo1 + sizeof(otherinfo1_data);
			break;
		case 3:
			/* The hash name is not NUL-terminated. */
			flags = KDF_PRINT_SPARE;
			kckdfp->hashname = bogus_str;
			kckdfp->otherinfo = kckdfp_otherinfo1;
			break;
		case 4:
			flags |= KDF_PRINT_SPARE;
			kckdfp->hashname = kckdfp_short_hash;
			kckdfp->otherinfo = kckdfp_otherinfo1;
			break;
		case 5:
			kckdfp->hashname = kckdfp_short_hash;
			kckdfp->otherinfo = kckdfp_otherinfo2;
			break;
		}

		do_keyctl(ARG_STR(KEYCTL_DH_COMPUTE), 4,
			  A_DH_PARAMS,
			  X_BUF("buffer", bogus_buf2, (size_t) -1, false),
			  X_UNSIGNED("buflen", (kernel_ulong_t) -1),
			  ARG(p_kdf_params, "kdf_params", (uintptr_t) kckdfp,
			      .size = flags, .exit = true));
	}

	/* KEYCTL_PKEY_QUERY */
	do_keyctl(ARG_STR(KEYCTL_PKEY_QUERY), 4,
		  A_KEY("key", bogus_key2),
		  A_HEX("reserved", bogus_size),
		  A_ADDR("params", bogus_str),
		  X_ADDR("info", NULL));

	struct keyctl_pkey_query query;
	do_keyctl(ARG_STR(KEYCTL_PKEY_QUERY), 4,
		  A_KEY_SPEC("key", ARG_STR(KEY_SPEC_THREAD_KEYRING)),
		  A_HEX("reserved", 0),
		  A_STR("params", "x\377\0\1"),
		  X_ADDR("info", &query));

	/*
	 * KEYCTL_PKEY_ENCRYPT, KEYCTL_PKEY_DECRYPT, KEYCTL_PKEY_SIGN,
	 * KEYCTL_PKEY_VERIFY
	 */
	static const struct {
		int op;
		const char *str;
		bool out;
	} pkey_ops[] = {
		{ ARG_STR(KEYCTL_PKEY_ENCRYPT),	true },
		{ ARG_STR(KEYCTL_PKEY_DECRYPT),	true },
		{ ARG_STR(KEYCTL_PKEY_SIGN),	true },
		{ ARG_STR(KEYCTL_PKEY_VERIFY),	false },
	};
	static const char pkey_str1[] = STR32 "xxx";
	static const char pkey_str2[] = "\1\2HAI\255\0\1";
	static const struct {
		struct keyctl_pkey_params params;
		const char *key_str;
		bool print_spare;
		bool in2_readable;
	} pkey_vecs[] = {
		{ { KEY_SPEC_PROCESS_KEYRING, 0, { .out_len = 0 } },
		  "KEY_SPEC_PROCESS_KEYRING", false, true },
		{ { 0, 0, { .out_len = 0 }, .__spare = { 1 } },
		  NULL, true, true },
		{ { 0xdeadc0deU, 10, { .out_len = 10 },
		    .__spare = { 0, 0xfacefeed } },
		  NULL, true, false },
		{ { 0xdeadc0deU, sizeof(pkey_str1),
		    { .out_len = sizeof(pkey_str2) - 1 },
		    .__spare = { [6] = 0xdec0ded } },
		  NULL, true, true },
	};

	char *pkey1 = tail_memdup(pkey_str1, sizeof(pkey_str1) - 1);
	char *pkey2 = tail_memdup(pkey_str2, sizeof(pkey_str2) - 1);
	TAIL_ALLOC_OBJECT_CONST_PTR(struct keyctl_pkey_params, pkey_params);

	for (size_t i = 0; i < ARRAY_SIZE(pkey_ops); ++i) {
		if (!pkey_ops[i].out) {
			do_keyctl(pkey_ops[i].op, pkey_ops[i].str, 4,
				  A_ADDR("params", NULL),
				  A_ADDR("info", NULL),
				  A_ADDR("in", NULL),
				  A_ADDR("in2", NULL));
			do_keyctl(pkey_ops[i].op, pkey_ops[i].str, 4,
				  A_ADDR("params", (uint32_t *) pkey_params + 1),
				  A_STR("info", "x\377\0\1"),
				  A_ADDR("in", pkey1),
				  A_ADDR("in2", pkey2));
		}

		for (size_t j = 0; j < ARRAY_SIZE(pkey_vecs); j++) {
			memcpy(pkey_params, &pkey_vecs[j].params,
			       sizeof(*pkey_params));

			const struct arg params =
				ARG(p_pkey_params, "params",
				    (uintptr_t) pkey_params,
				    .sym = pkey_vecs[j].key_str,
				    .flag = pkey_ops[i].out,
				    .size = pkey_vecs[j].print_spare);
			const struct arg in =
				A_MEM("in", pkey1, pkey_params->in_len);
			struct arg op2;

			if (pkey_ops[i].out)
				op2 = X_BUF("out", pkey2,
					    pkey_params->out_len, false);
			else if (pkey_vecs[j].in2_readable)
				op2 = A_MEM("in2", pkey2,
					    pkey_params->in2_len);
			else
				op2 = A_ADDR("in2", pkey2);

			do_keyctl(pkey_ops[i].op, pkey_ops[i].str, 4,
				  params, A_STR("info", ""), in, op2);
		}
	}

	/* KEYCTL_MOVE */
	static const kernel_ulong_t move_flags[] = {
		(kernel_ulong_t) 0xbadc0ded00000000ULL,
		1,
		(kernel_ulong_t) 0xbadc0ded00000001ULL,
		(kernel_ulong_t) 0xfffffffffffffffeULL,
		(kernel_ulong_t) 0xffffffffffffffffULL
	};

	for (size_t i = 0; i < ARRAY_SIZE(move_flags); ++i) {
		const struct arg bogus = A_KEY("", 0xbadc0ded);
		const struct arg spec =
			A_KEY_SPEC("", ARG_STR(KEY_SPEC_THREAD_KEYRING));
		struct arg key = i % 2 ? spec : bogus;
		struct arg from = i % 2 ? bogus : spec;
		struct arg to = key;

		key.name = "key";
		from.name = "from_keyring";
		to.name = "to_keyring";

		do_keyctl(ARG_STR(KEYCTL_MOVE), 4, key, from, to,
			  A_FLAGS("flags", move_flags[i], keyctl_move_flags,
				  "KEYCTL_MOVE_???"));
	}

	/* KEYCTL_CAPABILITIES */
	unsigned char *caps1 = tail_alloc(1);
	unsigned char *caps2 = tail_alloc(2);

#define X_CAPS(ptr_, size_) \
	ARG(p_caps, "buffer", (uintptr_t) (ptr_), .size = (size_), \
	    .exit = true)

	do_keyctl(ARG_STR(KEYCTL_CAPABILITIES), 2,
		  X_ADDR("buffer", NULL),
		  X_UNSIGNED("buflen", bogus_size));
	do_keyctl(ARG_STR(KEYCTL_CAPABILITIES), 2,
		  X_CAPS(caps1, 1),
		  X_UNSIGNED("buflen",
			     (kernel_ulong_t) 0xbadc0ded00000001ULL));

	/* More than two bytes of capabilities do not fit into the buffer. */
	if (do_keyctl(ARG_STR(KEYCTL_CAPABILITIES), 2,
		      X_CAPS(caps1, 1),
		      X_UNSIGNED("buflen", 2)) <= 2)
		do_keyctl(ARG_STR(KEYCTL_CAPABILITIES), 2,
			  X_CAPS(caps2, 2),
			  X_UNSIGNED("buflen", 2));

	jsonl_exit_event(0);
	return 0;
}
