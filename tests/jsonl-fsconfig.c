/*
 * Check JSONL output of the fsconfig syscall with -y.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"
#include "scno.h"

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <linux/mount.h>

static bool merged;

static const char path_full[] = "/dev/full";
static const int max_string_size = 256;
static const int max_blob_size = 300;
static const int huge_blob_size = 1024 * 1024 + 1;
static const char *fd_path;
/* When set, an aux argument of AT_FDCWD is a directory file descriptor. */
static const char *cwd;
static const void *efault;
static const char *empty;
static char *fname;
static char *key1;
static char *key;
static char *val1;
static char *val;
static char *blob1;
static char *blob;
static int fd;

enum val_kind {
	VAL_ADDR,
	VAL_STRING,
	VAL_BYTES,
	VAL_PATH
};

struct val {
	enum val_kind kind;
	const void *ptr;
	size_t len;
	bool truncated;
};

static struct val
v_addr(const void *ptr)
{
	return (struct val) { VAL_ADDR, ptr, 0, false };
}

/* A string cut at max_string_size bytes. */
static struct val
v_string(const char *str)
{
	size_t len = strnlen(str, max_string_size);

	return (struct val) { VAL_STRING, str, len, len == max_string_size };
}

/* A blob of len bytes cut at the string length limit. */
static struct val
v_bytes(const void *ptr, size_t len)
{
	return (struct val) {
		VAL_BYTES, ptr, MIN(len, max_blob_size), len > max_blob_size
	};
}

/* A path cut at PATH_MAX - 1 bytes. */
static struct val
v_path(const char *path)
{
	size_t len = strnlen(path, PATH_MAX);

	return (struct val) {
		VAL_PATH, path, MIN(len, PATH_MAX - 1), len == PATH_MAX
	};
}

static void
jsonl_val(const struct val *v)
{
	switch (v->kind) {
	case VAL_ADDR:
		jsonl_addr(v->ptr);
		break;
	case VAL_STRING:
		jsonl_string_value(v->ptr, v->len, v->truncated);
		break;
	case VAL_BYTES:
		jsonl_bytes_value(v->ptr, v->len, v->truncated);
		break;
	case VAL_PATH:
		jsonl_path_value_n(v->ptr, v->len, v->truncated);
		break;
	}
}

static void
jsonl_fs_fd(int n)
{
	jsonl_fd(n, n == fd ? fd_path : NULL, false);
}

/* The aux argument is printed as a file descriptor when aux_is_fd is set. */
static void
test_fsconfig(const unsigned int fs_fd, const unsigned int cmd,
	      const char *cmd_str, const struct val k, const struct val v,
	      const unsigned int aux, bool aux_is_fd)
{
	const kernel_ulong_t fill = (kernel_ulong_t) 0xdefaced00000000ULL;
	const kernel_ulong_t bad = (kernel_ulong_t) 0xbadc0dedbadc0dedULL;
	const kernel_ulong_t arg1 = fill | fs_fd;
	const kernel_ulong_t arg2 = fill | cmd;
	const kernel_ulong_t arg3 = (uintptr_t) k.ptr;
	const kernel_ulong_t arg4 = (uintptr_t) v.ptr;
	const kernel_ulong_t arg5 = fill | aux;
	const char *side = merged ? "entry" : NULL;
	const long rc = syscall(__NR_fsconfig,
				arg1, arg2, arg3, arg4, arg5, bad);
	int saved_errno = errno;

	if (merged)
		jsonl_syscall_merged_open("fsconfig", __NR_fsconfig);
	else
		jsonl_syscall_open("fsconfig", __NR_fsconfig, true);

	jsonl_arg_open(0, "fd", side);
	jsonl_fs_fd(fs_fd);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(1, "cmd", side);
	jsonl_const(cmd, true, cmd_str, "FSCONFIG_???");
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(2, "key", side);
	jsonl_val(&k);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(3, "value", side);
	jsonl_val(&v);
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(4, "aux", side);
	if (cwd && (int) aux == AT_FDCWD) {
		jsonl_const_decimal(AT_FDCWD, "AT_FDCWD", NULL);
		printf(", \"fd_info\": {\"type\": \"path\", \"path\": ");
		jsonl_bytes(cwd);
		printf("}");
	} else if (aux_is_fd)
		jsonl_fs_fd(aux);
	else
		jsonl_decimal((int) aux);
	jsonl_arg_close(side);

	if (!merged) {
		printf("]}\n");
		jsonl_syscall_open("fsconfig", __NR_fsconfig, false);
	}
	printf("], ");
	errno = saved_errno;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

static void
test_fsconfig_cmd(const unsigned int cmd, const char *cmd_str)
{
	test_fsconfig(fd, cmd, cmd_str, v_addr(empty), v_addr(val),
		      -100, false);
	test_fsconfig(-100, cmd, cmd_str, v_addr(empty), v_addr(fd_path),
		      fd, false);
}

static void
test_fsconfig_set_flag(const unsigned int cmd, const char *cmd_str)
{
	test_fsconfig(fd, cmd, cmd_str, v_string(key), v_addr(val),
		      -100, false);
	test_fsconfig(fd, cmd, cmd_str, v_string(key1), v_addr(val),
		      -100, false);
	test_fsconfig(-100, cmd, cmd_str, v_string(key), v_addr(fd_path),
		      fd, false);
}

static void
test_fsconfig_set_string(const unsigned int cmd, const char *cmd_str)
{
	test_fsconfig(fd, cmd, cmd_str, v_string(key), v_string(val),
		      -100, false);
	test_fsconfig(fd, cmd, cmd_str, v_string(key1), v_string(val1),
		      -100, false);
	test_fsconfig(-100, cmd, cmd_str, v_string(key), v_string(fd_path),
		      fd, false);
}

static void
test_fsconfig_set_binary(const unsigned int cmd, const char *cmd_str)
{
	test_fsconfig(fd, cmd, cmd_str, v_string(key),
		      v_bytes(blob, max_blob_size), max_blob_size, false);
	test_fsconfig(fd, cmd, cmd_str, v_string(key1),
		      v_bytes(blob1, max_blob_size + 1), max_blob_size + 1,
		      false);
	test_fsconfig(fd, cmd, cmd_str, v_string(key),
		      v_bytes(empty, 0), 0, false);

	/* A blob of a negative or of a too large size is not fetched. */
	test_fsconfig(fd, cmd, cmd_str, v_string(key), v_addr(fname),
		      -100, false);
	test_fsconfig(fd, cmd, cmd_str, v_string(key), v_addr(fname),
		      huge_blob_size, false);

	test_fsconfig(-100, cmd, cmd_str, v_string(key),
		      v_bytes(fd_path, sizeof(path_full)), sizeof(path_full),
		      false);
	test_fsconfig(-100, cmd, cmd_str, v_string(key),
		      v_bytes(fname, fd), fd, false);
}

static void
test_fsconfig_set_path(const unsigned int cmd, const char *cmd_str)
{
	cwd = get_fd_path(get_dir_fd("."));

	/* The path is not NUL-terminated. */
	fill_memory_ex(fname, PATH_MAX, '0', 10);
	test_fsconfig(fd, cmd, cmd_str, v_string(key), v_path(fname),
		      AT_FDCWD, true);

	fname[PATH_MAX - 1] = '\0';
	test_fsconfig(fd, cmd, cmd_str, v_string(key), v_path(fname),
		      -1, true);
	test_fsconfig(-100, cmd, cmd_str, v_string(key), v_path(empty),
		      fd, true);
	test_fsconfig(-1, cmd, cmd_str, v_addr(NULL), v_path(fd_path),
		      AT_FDCWD, true);
	test_fsconfig(-1, cmd, cmd_str, v_addr(efault), v_addr(efault + 1),
		      fd, true);
	test_fsconfig(-100, cmd, cmd_str, v_string(key), v_path(fname),
		      -1, true);
	cwd = NULL;
}

static void
test_fsconfig_set_fd(const unsigned int cmd, const char *cmd_str)
{
	test_fsconfig(fd, cmd, cmd_str, v_string(key), v_addr(val),
		      -100, true);
	test_fsconfig(-100, cmd, cmd_str, v_string(key1), v_addr(NULL),
		      fd, true);
	test_fsconfig(-1, cmd, cmd_str, v_addr(efault), v_addr(efault + 1),
		      fd, true);
	test_fsconfig(-100, cmd, cmd_str, v_string(key), v_addr(fd_path),
		      -1, true);
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");
	skip_if_unavailable("/proc/self/fd/");

	fd_path = tail_memdup(path_full, sizeof(path_full));
	efault = fd_path + sizeof(path_full);
	empty = efault - 1;
	fname = tail_alloc(PATH_MAX);
	key1 = tail_alloc(max_string_size + 1);
	key = key1 + 1;
	val1 = tail_alloc(max_string_size + 1);
	val = val1 + 1;
	blob1 = tail_alloc(max_blob_size + 1);
	blob = blob1 + 1;

	fill_memory_ex(fname, PATH_MAX, '0', 10);
	fill_memory_ex(key1, max_string_size, 'a', 'z' - 'a' + 1);
	key1[max_string_size] = '\0';
	fill_memory_ex(val1, max_string_size, 'A', 'Z' - 'A' + 1);
	val1[max_string_size] = '\0';
	fill_memory_ex(blob, max_blob_size, '0', 10);
	blob[0] = 0;
	blob1[0] = 0;
	fd = open(fd_path, O_WRONLY);
	if (fd < 0)
		perror_msg_and_fail("open: %s", fd_path);

	/* unknown commands */
	test_fsconfig(fd, 9, NULL, v_addr(empty), v_addr(val), -100, false);
	test_fsconfig(-100, -1, NULL, v_addr(empty), v_addr(fd_path),
		      fd, false);

	test_fsconfig_set_flag(ARG_STR(FSCONFIG_SET_FLAG));
	test_fsconfig_set_string(ARG_STR(FSCONFIG_SET_STRING));
	test_fsconfig_set_binary(ARG_STR(FSCONFIG_SET_BINARY));
	test_fsconfig_set_path(ARG_STR(FSCONFIG_SET_PATH));
	test_fsconfig_set_path(ARG_STR(FSCONFIG_SET_PATH_EMPTY));
	test_fsconfig_set_fd(ARG_STR(FSCONFIG_SET_FD));
	test_fsconfig_cmd(ARG_STR(FSCONFIG_CMD_CREATE));
	test_fsconfig_cmd(ARG_STR(FSCONFIG_CMD_RECONFIGURE));
	test_fsconfig_cmd(ARG_STR(FSCONFIG_CMD_CREATE_EXCL));

	jsonl_exit_event(0);
	return 0;
}
