/*
 * Check JSONL output of v4l2 ioctl struct arguments: split structs of
 * failed ioctls and the fourcc, fract, scalar-array, and compound control-ID
 * values.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#ifdef __NR_ioctl

# include "jsonl.h"
# include "kernel_v4l2_types.h"

# include <linux/ioctl.h>
# include <sys/ioctl.h>

static bool merged;

/* The "dir" flags field of an ioctl op, derived from its command code. */
static void
emit_dir(unsigned int code)
{
	const unsigned int dir = _IOC_DIR(code);
	bool first = true;

	printf(", \"dir\": {");
	jsonl_flags_open(dir);
	if (dir == _IOC_NONE) {
		jsonl_flags_const(_IOC_NONE, "_IOC_NONE", first);
	} else {
		if (dir & _IOC_READ) {
			jsonl_flags_const(_IOC_READ, "_IOC_READ", first);
			first = false;
		}
		if (dir & _IOC_WRITE)
			jsonl_flags_const(_IOC_WRITE, "_IOC_WRITE", first);
	}
	jsonl_flags_close();
	printf("}");
}

/* An ioctl_op value: raw code, resolved command name, and the _IOC parts. */
static void
emit_op(unsigned int code, const char *cmd)
{
	printf("\"type\": \"ioctl_op\", \"raw\": \"%#x\", "
	       "\"cmds\": [{\"type\": \"ioctl_cmd\", \"name\": \"%s\"}]",
	       code, cmd);
	emit_dir(code);
	printf(", \"ioc_type\": \"%#x\", \"ioc_nr\": \"%#x\","
	       " \"ioc_size\": \"%#x\"",
	       _IOC_TYPE(code), _IOC_NR(code), _IOC_SIZE(code));
}

/* The index field of a v4l2_input (VIDIOC_ENUMINPUT), a plain split struct. */
# define INPUT_INDEX 7
static void
emit_input_fields(void)
{
	printf("\"index\": {\"type\": \"unsigned\", \"raw\": \"%d\"}",
	       INPUT_INDEX);
}

/*
 * A VBI v4l2_format (VIDIOC_S_FMT): the sample_format fourcc, the start and
 * count scalar arrays (array elements are objects), and the flags value.
 */
static void
emit_vbi_fields(void)
{
	printf("\"type\": {\"type\": \"const\", \"raw\": \"%#x\","
	       " \"sym\": \"V4L2_BUF_TYPE_VBI_CAPTURE\"}, "
	       "\"fmt.vbi\": {\"type\": \"struct\", \"fields\": {"
	       "\"sampling_rate\": {\"type\": \"unsigned\", \"raw\": \"286331153\"}, "
	       "\"offset\": {\"type\": \"unsigned\", \"raw\": \"572662306\"}, "
	       "\"samples_per_line\": {\"type\": \"unsigned\", \"raw\": \"858993459\"}, "
	       "\"sample_format\": {\"type\": \"fourcc\", \"raw\": \"0x59455247\","
	       " \"sym\": \"V4L2_PIX_FMT_GREY\"}, "
	       "\"start\": {\"type\": \"array\", \"elems\": ["
	       "{\"type\": \"decimal\", \"raw\": \"68\"}, "
	       "{\"type\": \"decimal\", \"raw\": \"85\"}]}, "
	       "\"count\": {\"type\": \"array\", \"elems\": ["
	       "{\"type\": \"unsigned\", \"raw\": \"102\"}, "
	       "{\"type\": \"unsigned\", \"raw\": \"119\"}]}, "
	       "\"flags\": {\"type\": \"flags\", \"raw\": \"0x1\", \"groups\": ["
	       "{\"raw\": \"0x1\", \"group_type\": \"flags\", \"elems\": ["
	       "{\"type\": \"const\", \"raw\": \"0x1\","
	       " \"sym\": \"V4L2_VBI_UNSYNC\"}]}]}}}",
	       V4L2_BUF_TYPE_VBI_CAPTURE);
}

/*
 * The id of a v4l2_queryctrl (VIDIOC_QUERYCTRL): a compound flags value,
 * with a group for the control-ID flags and another one for the control id.
 */
# define QUERYCTRL_ID (0x80000000u | 0x00febdbcu)
static void
emit_queryctrl_fields(void)
{
	printf("\"id\": {");
	jsonl_flags_groups_open(QUERYCTRL_ID);
	jsonl_flags_group_open(0x80000000u, true);
	jsonl_flags_const(0x80000000u, "V4L2_CTRL_FLAG_NEXT_CTRL", true);
	jsonl_flags_group_close();
	jsonl_flags_group_open(0x00febdbcu, false);
	printf("{");
	jsonl_const(0x00febdbcu, true, NULL, "V4L2_CID_???");
	printf("}");
	jsonl_flags_group_close();
	jsonl_flags_groups_close();
	printf("}");
}

/* A capture v4l2_streamparm (VIDIOC_S_PARM): the timeperframe fract value. */
static void
emit_sparm_fields(void)
{
	printf("\"type\": {\"type\": \"const\", \"raw\": \"%#x\","
	       " \"sym\": \"V4L2_BUF_TYPE_VIDEO_CAPTURE\"}, "
	       "\"parm.capture\": {\"type\": \"struct\", \"fields\": {"
	       "\"capability\": {\"type\": \"flags\", \"raw\": \"0\", \"groups\": ["
	       "{\"raw\": \"0\", \"group_type\": \"flags\", \"elems\": []}]}, "
	       "\"capturemode\": {\"type\": \"flags\", \"raw\": \"0\", \"groups\": ["
	       "{\"raw\": \"0\", \"group_type\": \"flags\", \"elems\": []}]}, "
	       "\"timeperframe\": {\"type\": \"fract\", \"numerator\": \"30\","
	       " \"denominator\": \"1\"}, "
	       "\"extendedmode\": {\"type\": \"hex\", \"raw\": \"0\"}, "
	       "\"readbuffers\": {\"type\": \"unsigned\", \"raw\": \"0\"}}}",
	       V4L2_BUF_TYPE_VIDEO_CAPTURE);
}

struct testcase {
	unsigned int cmd;
	const char *cmd_name;
	void (*emit_fields)(void);
	bool split;
};

static const struct testcase cases[] = {
	{ VIDIOC_ENUMINPUT, "VIDIOC_ENUMINPUT", emit_input_fields, true },
	{ VIDIOC_S_FMT, "VIDIOC_S_FMT", emit_vbi_fields, false },
	{ VIDIOC_QUERYCTRL, "VIDIOC_QUERYCTRL", emit_queryctrl_fields, true },
	{ VIDIOC_S_PARM, "VIDIOC_S_PARM", emit_sparm_fields, false },
};

static void
issue_ioctls(void)
{
	static struct v4l2_input input;
	input.index = INPUT_INDEX;
	ioctl(-1, VIDIOC_ENUMINPUT, &input);

	static struct v4l2_format fmt;
	fmt.type = V4L2_BUF_TYPE_VBI_CAPTURE;
	fmt.fmt.vbi.sampling_rate = 0x11111111;
	fmt.fmt.vbi.offset = 0x22222222;
	fmt.fmt.vbi.samples_per_line = 0x33333333;
	fmt.fmt.vbi.sample_format = 0x59455247;
	fmt.fmt.vbi.start[0] = 0x44;
	fmt.fmt.vbi.start[1] = 0x55;
	fmt.fmt.vbi.count[0] = 0x66;
	fmt.fmt.vbi.count[1] = 0x77;
	fmt.fmt.vbi.flags = 0x1;
	ioctl(-1, VIDIOC_S_FMT, &fmt);

	static struct v4l2_queryctrl qc;
	qc.id = QUERYCTRL_ID;
	ioctl(-1, VIDIOC_QUERYCTRL, &qc);

	static struct v4l2_streamparm sp;
	sp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	sp.parm.capture.timeperframe.numerator = 30;
	sp.parm.capture.timeperframe.denominator = 1;
	ioctl(-1, VIDIOC_S_PARM, &sp);
}

static void
emit_case(const struct testcase *c)
{
	if (merged) {
		jsonl_syscall_merged_open("ioctl", __NR_ioctl);
		jsonl_arg_open(0, "fd", "entry");
		jsonl_fd(-1, NULL, false);
		jsonl_arg_close("entry");
		printf(", ");
		jsonl_arg_open(1, "op", "entry");
		emit_op(c->cmd, c->cmd_name);
		jsonl_arg_close("entry");
		printf(", ");
		jsonl_arg_open(2, "argp", "entry");
		printf("\"type\": \"struct\", \"fields\": {");
		c->emit_fields();
		printf("}%s}}", c->split ? ", \"split\": true" : "");
		printf("], ");
		errno = EBADF;
		jsonl_return("decimal", -1);
		printf("}\n");
		return;
	}

	jsonl_syscall_open("ioctl", __NR_ioctl, true);
	jsonl_arg_open(0, "fd", NULL);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(NULL);
	printf(", ");
	jsonl_arg_open(1, "op", NULL);
	emit_op(c->cmd, c->cmd_name);
	jsonl_arg_close(NULL);
	printf(", ");
	jsonl_arg_open(2, "argp", NULL);
	printf("\"type\": \"struct\", \"fields\": {");
	c->emit_fields();
	printf("}%s", c->split ? ", \"split\": true" : "");
	jsonl_arg_close(NULL);
	printf("]}\n");

	jsonl_syscall_open("ioctl", __NR_ioctl, false);
	printf("], ");
	errno = EBADF;
	jsonl_return("decimal", -1);
	printf("}\n");
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	issue_ioctls();

	for (unsigned int i = 0; i < ARRAY_SIZE(cases); ++i)
		emit_case(&cases[i]);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
