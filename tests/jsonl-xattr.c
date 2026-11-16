/*
 * Check JSONL output of the get, set, list, and remove xattr syscalls.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_setxattr && defined __NR_getxattr \
 && defined __NR_listxattr && defined __NR_removexattr

# include "jsonl.h"

# include <fcntl.h>
# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>
# include <linux/xattr.h>

# ifndef XATTR_SIZE_MAX
#  define XATTR_SIZE_MAX 65536
# endif

# define SC(name_) #name_, __NR_##name_

static bool merged;

static const char *sc_name;
static long sc_nr;
static bool sc_exiting;
static bool sc_first;

static const char *
sc_side(void)
{
	return merged ? (sc_exiting ? "exit" : "entry") : NULL;
}

static void
sc_begin(const char *name, long nr)
{
	sc_name = name;
	sc_nr = nr;
	sc_exiting = false;
	sc_first = true;

	if (merged)
		jsonl_syscall_merged_open(name, nr);
	else
		jsonl_syscall_open(name, nr, true);
}

static void
sc_exit(void)
{
	sc_exiting = true;
	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open(sc_name, sc_nr, false);
		sc_first = true;
	}
}

static void
sc_end(long rc, int err)
{
	if (!sc_exiting)
		sc_exit();
	printf("], ");
	errno = err;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

static void
arg_open(unsigned int index, const char *name)
{
	if (!sc_first)
		printf(", ");
	sc_first = false;
	jsonl_arg_open(index, name, sc_side());
}

static void
arg_close(void)
{
	jsonl_arg_close(sc_side());
}

static void
arg_addr(unsigned int index, const char *name, const void *p)
{
	arg_open(index, name);
	jsonl_addr(p);
	arg_close();
}

/* The first len bytes of str, cut at the default string length limit. */
static void
arg_string_n(unsigned int index, const char *name, const char *str, size_t len)
{
	arg_open(index, name);
	jsonl_string_value(str, MIN(len, DEFAULT_STRLEN),
			   len > DEFAULT_STRLEN);
	arg_close();
}

static void
arg_name(const char *name)
{
	if (name)
		arg_string_n(1, "name", name, strlen(name));
	else
		arg_addr(1, "name", NULL);
}

/* An attribute value: its terminating NUL, if any, is not printed. */
static void
arg_value(const char *value, size_t size)
{
	if (size && !value[size - 1])
		--size;
	arg_string_n(2, "value", value, size);
}

static void
arg_size(unsigned int index, size_t size)
{
	arg_open(index, "size");
	jsonl_unsigned(size);
	arg_close();
}

/* A file is specified by its path, or by fd when path is NULL. */
static void
arg_file(int fd, const char *path)
{
	if (path) {
		arg_open(0, "pathname");
		jsonl_path_value(path);
	} else {
		arg_open(0, "fd");
		jsonl_fd(fd, NULL, false);
	}
	arg_close();
}

static void
arg_flags(unsigned int flags)
{
	static const struct strval32 known[] = {
		{ ARG_STR(XATTR_CREATE) },
		{ ARG_STR(XATTR_REPLACE) }
	};

	arg_open(4, "flags");
	jsonl_flags(flags, known, ARRAY_SIZE(known), NULL);
	arg_close();
}

/* readable is false when the value is expected to be printed as a pointer. */
static void
test_set(const char *sc, long nr, int fd, const char *path, const char *name,
	 const char *value, size_t size, unsigned int flags, bool readable)
{
	long rc = path ? syscall(nr, path, name, value, size, flags)
		       : syscall(nr, fd, name, value, size, flags);
	int err = errno;

	sc_begin(sc, nr);
	arg_file(fd, path);
	arg_name(name);
	if (readable)
		arg_value(value, size);
	else
		arg_addr(2, "value", value);
	arg_size(3, size);
	arg_flags(flags);
	sc_end(rc, err);
}

static long
test_get(const char *sc, long nr, int fd, const char *path, const char *name,
	 const char *value, size_t size)
{
	long rc = path ? syscall(nr, path, name, value, size)
		       : syscall(nr, fd, name, value, size);
	int err = errno;

	sc_begin(sc, nr);
	arg_file(fd, path);
	arg_name(name);
	if (size) {
		sc_exit();
		if (rc < 0)
			arg_addr(2, "value", value);
		else
			arg_value(value, rc);
	} else {
		arg_addr(2, "value", value);
	}
	arg_size(3, size);
	sc_end(rc, err);

	return rc;
}

static void
test_list(const char *sc, long nr, int fd, const char *path,
	  const char *list, size_t size)
{
	long rc = path ? syscall(nr, path, list, size)
		       : syscall(nr, fd, list, size);
	int err = errno;

	sc_begin(sc, nr);
	arg_file(fd, path);
	sc_exit();
	if (size && rc >= 0)
		arg_string_n(1, "list", list, rc);
	else
		arg_addr(1, "list", list);
	arg_size(2, size);
	sc_end(rc, err);
}

static void
test_remove(const char *sc, long nr, int fd, const char *path,
	    const char *name)
{
	long rc = path ? syscall(nr, path, name) : syscall(nr, fd, name);
	int err = errno;

	sc_begin(sc, nr);
	arg_file(fd, path);
	arg_name(name);
	sc_end(rc, err);
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	static const char name[] = "user.strace.test.xattr";
	static const char c_value[] = "foo\0bar";

	const char *const z_value = tail_memdup(c_value, sizeof(c_value));
	char *const efault = tail_alloc(1) + 1;
	const char *const value = tail_memdup(c_value, sizeof(c_value) - 1);
	const unsigned int big_size = XATTR_SIZE_MAX;
	char *const big = tail_alloc(big_size);

	int fd = open(".", O_RDONLY);
	if (fd < 0)
		perror_msg_and_fail("open");

	test_set(SC(fsetxattr), -1, NULL, NULL, NULL, 0, XATTR_CREATE, false);
	test_set(SC(fsetxattr), -1, NULL, NULL, z_value, 0, XATTR_CREATE,
		 true);
	test_set(SC(fsetxattr), -1, NULL, name, big, XATTR_SIZE_MAX + 1,
		 XATTR_CREATE, false);

	fill_memory(big, big_size);
	test_set(SC(fsetxattr), -1, NULL, name, big, big_size, XATTR_CREATE,
		 true);

	/* The last byte of the value is not readable. */
	test_set(SC(fsetxattr), -1, NULL, name, value, sizeof(c_value),
		 XATTR_CREATE, false);
	test_set(SC(fsetxattr), -1, NULL, name, value, sizeof(c_value) - 1,
		 XATTR_CREATE | XATTR_REPLACE, true);
	test_set(SC(fsetxattr), -1, NULL, name, z_value, sizeof(c_value),
		 XATTR_REPLACE | 0xfffffff8U, true);

	test_set(SC(setxattr), -1, ".", name, z_value, sizeof(c_value),
		 XATTR_CREATE, true);

	/* The value ends with a NUL byte that is not printed. */
	test_get(SC(getxattr), -1, ".", name, big, big_size);
	test_get(SC(fgetxattr), fd, NULL, name, big, big_size);

	test_set(SC(lsetxattr), -1, ".", name, value, sizeof(c_value) - 1,
		 XATTR_REPLACE, true);
	test_set(SC(fsetxattr), fd, NULL, name, value, sizeof(c_value) - 1,
		 XATTR_REPLACE, true);

	test_get(SC(fgetxattr), -1, NULL, name, efault, 4);
	test_get(SC(getxattr), -1, ".", name, big, 0);
	test_get(SC(lgetxattr), -1, ".", name, big, big_size);
	test_get(SC(fgetxattr), fd, NULL, name, big, big_size);
	test_get(SC(fgetxattr), fd, NULL, NULL, big, big_size);
	test_get(SC(getxattr), -1, ".", name, efault, 1);

	test_list(SC(flistxattr), -1, NULL, efault, 4);
	test_list(SC(listxattr), -1, ".", big, 0);
	test_list(SC(llistxattr), -1, ".", big, big_size);
	test_list(SC(flistxattr), fd, NULL, big, big_size);
	test_list(SC(listxattr), -1, ".", efault, 1);

	test_remove(SC(fremovexattr), -1, NULL, name);
	test_remove(SC(removexattr), -1, ".", name);
	test_remove(SC(lremovexattr), -1, ".", name);
	test_remove(SC(fremovexattr), fd, NULL, name);
	test_remove(SC(removexattr), -1, ".", NULL);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_setxattr && __NR_getxattr"
		    " && __NR_listxattr && __NR_removexattr")

#endif
