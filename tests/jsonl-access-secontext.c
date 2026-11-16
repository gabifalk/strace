/*
 * Check JSONL output of security contexts of the access syscall.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_access && defined HAVE_SELINUX_RUNTIME

# include "jsonl.h"

# include <fcntl.h>
# include <stdio.h>
# include <string.h>
# include <unistd.h>

# define TEST_SECONTEXT
# include "secontext.h"

static bool merged;
static const char *my_secontext;

/*
 * Print the "secontext" field described by a string in the traditional
 * format, "[context]" or "[context!!expected]" with optional spaces
 * around it, or nothing if the string is empty.
 */
static void
jsonl_secontext(const char *prefix, const char *str, const char *suffix)
{
	const char *ctx = strchr(str, '[');
	const char *end = strrchr(str, ']');

	if (!ctx || !end)
		return;
	++ctx;

	static const char delim[] = "!!";
	const char *expected = strstr(ctx, delim);

	printf("%s\"secontext\": {\"context\": ", prefix);
	jsonl_bytes_n(ctx, (expected ?: end) - ctx);
	if (expected) {
		expected += sizeof(delim) - 1;
		printf(", \"expected\": ");
		jsonl_bytes_n(expected, end - expected);
	}
	printf("}%s", suffix);
}

static void
syscall_open(bool entering)
{
	printf("{\"event\": \"syscall\", "
	       "\"pid\": {\"type\": \"tid\", \"raw\": \"%d\"}, ",
	       (int) getpid());
	jsonl_secontext("", my_secontext, ", ");
	printf("\"syscall\": {\"type\": \"syscall\", \"name\": \"access\","
	       " \"scno\": \"%ld\", \"personality\": %d}, ",
	       (long) __NR_access, JSONL_PERSONALITY);
	if (!merged)
		printf("\"entering\": %s, ", entering ? "true" : "false");
	printf("\"args\": [");
}

static void
test_access(const char *path, const char *path_secontext, unsigned int mode)
{
	static const struct strval32 modes[] = {
		{ ARG_STR(F_OK) },
		{ ARG_STR(R_OK) },
		{ ARG_STR(W_OK) },
		{ ARG_STR(X_OK) }
	};
	const char *side = merged ? "entry" : NULL;

	long rc = syscall(__NR_access, path, mode);
	int err = errno;

	syscall_open(true);

	jsonl_arg_open(0, "pathname", side);
	jsonl_path_value(path);
	jsonl_secontext(", ", path_secontext, "");
	jsonl_arg_close(side);
	printf(", ");

	jsonl_arg_open(1, "mode", side);
	jsonl_flags(mode, modes, ARRAY_SIZE(modes), NULL);
	jsonl_arg_close(side);

	if (!merged) {
		printf("]}\n");
		syscall_open(false);
	}
	printf("], ");
	errno = err;
	jsonl_return("unsigned", rc);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	/*
	 * Make sure the current workdir of the tracee
	 * is different from the current workdir of the tracer.
	 */
	create_and_enter_subdir("access_subdir");

	my_secontext = SECONTEXT_PID_MY();

	static const char sample[] = "access_sample";
	(void) unlink(sample);
	if (open(sample, O_CREAT|O_RDONLY, 0400) == -1)
		perror_msg_and_fail("open: %s", sample);

	const char *sample_secontext = SECONTEXT_FILE(sample);
	bool skip = !*my_secontext && !*sample_secontext;

	if (!skip)
		test_access(sample, sample_secontext, F_OK);

	if (unlink(sample))
		perror_msg_and_fail("unlink: %s", sample);

	if (!skip)
		test_access(sample, "", R_OK|W_OK|X_OK);

	leave_and_remove_subdir();

	if (skip)
		error_msg_and_skip("security contexts are not available");

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_access && HAVE_SELINUX_RUNTIME")

#endif
