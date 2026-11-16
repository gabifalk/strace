/*
 * Check JSONL output of GPIO_* commands of ioctl syscall.
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

# include <stdint.h>
# include <stdio.h>
# include <string.h>
# include <sys/ioctl.h>
# include <linux/ioctl.h>
# include <linux/gpio.h>

# define UNK_GPIO_FLAG 0x8000
# define MAX_STRLEN 32

static bool merged;
static bool exiting;
static long ioctl_rc;
static int ioctl_errno;

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

/*
 * An ioctl_op value: raw code, the names of the commands it resolves to,
 * and the _IOC parts.
 */
static void
emit_op(unsigned int code, const char *cmd, const char *cmd2)
{
	printf("\"type\": \"ioctl_op\", \"raw\": \"%#x\"", code);
	if (cmd) {
		printf(", \"cmds\": [{\"type\": \"ioctl_cmd\","
		       " \"name\": \"%s\"}", cmd);
		if (cmd2)
			printf(", {\"type\": \"ioctl_cmd\","
			       " \"name\": \"%s\"}", cmd2);
		printf("]");
	}
	emit_dir(code);
	printf(", \"ioc_type\": \"%#x\", \"ioc_nr\": \"%#x\","
	       " \"ioc_size\": \"%#x\"",
	       _IOC_TYPE(code), _IOC_NR(code), _IOC_SIZE(code));
}

/* Invoke the ioctl and open its event up to the end of the op argument. */
static void
ioctl_call(unsigned long code, const char *cmd, const char *cmd2,
	   const void *arg)
{
	const char *side = merged ? "entry" : NULL;

	ioctl_rc = ioctl(-1, code, arg);
	ioctl_errno = errno;
	exiting = false;

	if (merged)
		jsonl_syscall_merged_open("ioctl", __NR_ioctl);
	else
		jsonl_syscall_open("ioctl", __NR_ioctl, true);

	jsonl_arg_open(0, "fd", side);
	jsonl_fd(-1, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "op", side);
	emit_op(code, cmd, cmd2);
	jsonl_arg_close(side);
}

static const char *
argp_side(void)
{
	return merged ? (exiting ? "exit" : "entry") : NULL;
}

/* Open the third argument, printed on entering or on exiting. */
static void
argp_open(const char *name, bool on_exit)
{
	exiting = on_exit;
	if (on_exit && !merged) {
		printf("]}\n");
		jsonl_syscall_open("ioctl", __NR_ioctl, false);
	} else {
		printf(", ");
	}
	jsonl_arg_open(2, name, argp_side());
}

static void
argp_close(void)
{
	jsonl_arg_close(argp_side());
}

static void
ioctl_end(void)
{
	if (!merged && !exiting) {
		printf("]}\n");
		jsonl_syscall_open("ioctl", __NR_ioctl, false);
	}
	printf("], ");
	errno = ioctl_errno;
	jsonl_return("decimal", ioctl_rc);
	printf("}\n");
}


static const struct strval32 handle_flags[] = {
	{ ARG_STR(GPIOHANDLE_REQUEST_INPUT) },
	{ ARG_STR(GPIOHANDLE_REQUEST_OUTPUT) },
	{ ARG_STR(GPIOHANDLE_REQUEST_ACTIVE_LOW) },
	{ ARG_STR(GPIOHANDLE_REQUEST_OPEN_DRAIN) },
	{ ARG_STR(GPIOHANDLE_REQUEST_OPEN_SOURCE) },
	{ ARG_STR(GPIOHANDLE_REQUEST_BIAS_PULL_UP) },
	{ ARG_STR(GPIOHANDLE_REQUEST_BIAS_PULL_DOWN) },
	{ ARG_STR(GPIOHANDLE_REQUEST_BIAS_DISABLE) },
};

static const struct strval32 event_flags[] = {
	{ ARG_STR(GPIOEVENT_REQUEST_BOTH_EDGES) },
};

static const struct strval32 line_flags[] = {
	{ ARG_STR(GPIO_V2_LINE_FLAG_USED) },
	{ ARG_STR(GPIO_V2_LINE_FLAG_ACTIVE_LOW) },
	{ ARG_STR(GPIO_V2_LINE_FLAG_INPUT) },
	{ ARG_STR(GPIO_V2_LINE_FLAG_OUTPUT) },
	{ ARG_STR(GPIO_V2_LINE_FLAG_EDGE_RISING) },
	{ ARG_STR(GPIO_V2_LINE_FLAG_EDGE_FALLING) },
	{ ARG_STR(GPIO_V2_LINE_FLAG_OPEN_DRAIN) },
	{ ARG_STR(GPIO_V2_LINE_FLAG_OPEN_SOURCE) },
	{ ARG_STR(GPIO_V2_LINE_FLAG_BIAS_PULL_UP) },
	{ ARG_STR(GPIO_V2_LINE_FLAG_BIAS_PULL_DOWN) },
	{ ARG_STR(GPIO_V2_LINE_FLAG_BIAS_DISABLED) },
	{ ARG_STR(GPIO_V2_LINE_FLAG_EVENT_CLOCK_REALTIME) },
	{ ARG_STR(GPIO_V2_LINE_FLAG_EVENT_CLOCK_HTE) },
};

static void
field_unsigned(const char *name, bool first, uint64_t val)
{
	jsonl_field_open(name, first);
	jsonl_unsigned(val);
	jsonl_field_close();
}

static void
field_hex(const char *name, bool first, uint64_t val)
{
	jsonl_field_open(name, first);
	jsonl_hex(val);
	jsonl_field_close();
}

static void
field_string(const char *name, bool first, const char *str)
{
	jsonl_field_open(name, first);
	jsonl_string_value(str, strlen(str), false);
	jsonl_field_close();
}

static void
field_handle_flags(const char *name, bool first, uint32_t flags)
{
	jsonl_field_open(name, first);
	jsonl_flags(flags, handle_flags, ARRAY_SIZE(handle_flags),
		    "GPIOHANDLE_REQUEST_???");
	jsonl_field_close();
}

static void
field_line_flags(bool first, uint64_t flags)
{
	jsonl_field_open("flags", first);
	jsonl_flags(flags, line_flags, ARRAY_SIZE(line_flags),
		    "GPIO_V2_LINE_FLAG_???");
	jsonl_field_close();
}

/*
 * An array field of count integers of elem_size bytes each,
 * abbreviated after MAX_STRLEN elements.
 */
static void
field_array(const char *name, bool first, const void *data,
	    size_t elem_size, unsigned int count, bool hex)
{
	jsonl_field_open(name, first);
	jsonl_array_open();
	for (unsigned int i = 0; i < count && i < MAX_STRLEN; ++i) {
		const void *p = (const char *) data + i * elem_size;
		const uint32_t val = elem_size == sizeof(uint8_t)
				     ? *(const uint8_t *) p
				     : *(const uint32_t *) p;

		printf("%s{", i ? ", " : "");
		if (hex)
			jsonl_hex(val);
		else
			jsonl_unsigned(val);
		printf("}");
	}
	jsonl_array_close();
	if (count > MAX_STRLEN)
		jsonl_truncated();
	jsonl_field_close();
}

# define FIELD_ARRAY(name_, first_, arr_, count_, hex_)			\
	field_array((name_), (first_), (arr_), sizeof((arr_)[0]),	\
		    (count_), (hex_))

/* The argument is NULL or points to memory that cannot be read in full. */
static void
test_addr(unsigned int code, const char *cmd, const char *name,
	  bool on_exit, const void *addr)
{
	ioctl_call(code, cmd, NULL, addr);
	argp_open(name, on_exit);
	jsonl_addr(addr);
	argp_close();
	ioctl_end();
}

static void
test_gpiochip_info(void)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(struct gpiochip_info, p_chipinfo);

	p_chipinfo->lines = 0xca;
	strcpy(p_chipinfo->name, "chip name");
	strcpy(p_chipinfo->label, "chip label");

	test_addr(ARG_STR(GPIO_GET_CHIPINFO_IOCTL), "info", true, NULL);
	test_addr(ARG_STR(GPIO_GET_CHIPINFO_IOCTL), "info", true,
		  (char *) p_chipinfo + 1);
	test_addr(ARG_STR(GPIO_GET_CHIPINFO_IOCTL), "info", true, p_chipinfo);
}

static void
test_lineinfo(unsigned int code, const char *cmd,
	      const struct gpioline_info *info)
{
	ioctl_call(code, cmd, NULL, info);
	argp_open("info", false);
	jsonl_struct_open();
	field_unsigned("line_offset", true, info->line_offset);
	jsonl_struct_close();
	argp_close();
	ioctl_end();
}

static void
test_gpioline_info(void)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(struct gpioline_info, p_lineinfo);

	p_lineinfo->line_offset = 0x32;
	p_lineinfo->flags = GPIOLINE_FLAG_ACTIVE_LOW|GPIOLINE_FLAG_OPEN_DRAIN;
	strcpy(p_lineinfo->name, "line name");
	strcpy(p_lineinfo->consumer, "line consumer");

	test_addr(ARG_STR(GPIO_GET_LINEINFO_IOCTL), "info", false, NULL);
	test_addr(ARG_STR(GPIO_GET_LINEINFO_IOCTL), "info", false,
		  (char *) p_lineinfo + 1);

	test_lineinfo(ARG_STR(GPIO_GET_LINEINFO_IOCTL), p_lineinfo);
	test_lineinfo(ARG_STR(GPIO_GET_LINEINFO_WATCH_IOCTL), p_lineinfo);

	p_lineinfo->flags = UNK_GPIO_FLAG;
	test_lineinfo(ARG_STR(GPIO_GET_LINEINFO_IOCTL), p_lineinfo);
}

static void
test_gpioline_info_unwatch(void)
{
	static const uint32_t offsets[] = { 0, 78 };
	TAIL_ALLOC_OBJECT_CONST_PTR(uint32_t, p_offset);

	test_addr(ARG_STR(GPIO_GET_LINEINFO_UNWATCH_IOCTL), "offset", false,
		  NULL);

	for (unsigned int i = 0; i < ARRAY_SIZE(offsets); ++i) {
		*p_offset = offsets[i];
		ioctl_call(ARG_STR(GPIO_GET_LINEINFO_UNWATCH_IOCTL), NULL,
			   p_offset);
		argp_open("offset", false);
		jsonl_struct_open();
		field_unsigned("offset", true, *p_offset);
		jsonl_struct_close();
		argp_close();
		ioctl_end();
	}
}

static void
test_handle_request(const struct gpiohandle_request *req)
{
	const unsigned int count = MIN(req->lines, GPIOHANDLES_MAX);

	ioctl_call(ARG_STR(GPIO_GET_LINEHANDLE_IOCTL), NULL, req);
	argp_open("request", false);
	jsonl_struct_open();
	field_unsigned("lines", true, req->lines);
	FIELD_ARRAY("lineoffsets", false, req->lineoffsets, count, false);
	field_handle_flags("flags", false, req->flags);
	FIELD_ARRAY("default_values", false, req->default_values, count,
		    false);
	field_string("consumer_label", false, req->consumer_label);
	jsonl_struct_close();
	argp_close();
	ioctl_end();
}

static void
test_gpiohandle_request(void)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(struct gpiohandle_request,
				    p_handle_request);

	p_handle_request->lines = 4;
	p_handle_request->lineoffsets[0] = 0x12;
	p_handle_request->lineoffsets[1] = 0x23;
	p_handle_request->lineoffsets[2] = 0x34;
	p_handle_request->lineoffsets[3] = 0x45;
	p_handle_request->default_values[0] = 0x0;
	p_handle_request->default_values[1] = 0x1;
	p_handle_request->default_values[2] = 0x2;
	p_handle_request->default_values[3] = 0x6;
	p_handle_request->flags =
		GPIOHANDLE_REQUEST_ACTIVE_LOW|GPIOHANDLE_REQUEST_OPEN_SOURCE;
	strcpy(p_handle_request->consumer_label, "line consumer");
	p_handle_request->fd = 0x64;

	test_addr(ARG_STR(GPIO_GET_LINEHANDLE_IOCTL), "request", false, NULL);
	test_addr(ARG_STR(GPIO_GET_LINEHANDLE_IOCTL), "request", false,
		  (char *) p_handle_request + 1);

	test_handle_request(p_handle_request);

	/* lines > GPIOHANDLES_MAX */
	p_handle_request->lines = GPIOHANDLES_MAX + 1;
	for (int i = 0; i < GPIOHANDLES_MAX; i++) {
		p_handle_request->lineoffsets[i] = i + 1;
		p_handle_request->default_values[i] = 2 * i + 1;
	}
	test_handle_request(p_handle_request);
}

static void
test_gpioevent_request(void)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(struct gpioevent_request, p_event_request);

	p_event_request->lineoffset = 4;
	p_event_request->handleflags =
		GPIOHANDLE_REQUEST_ACTIVE_LOW|GPIOHANDLE_REQUEST_OPEN_SOURCE;
	p_event_request->eventflags = GPIOEVENT_REQUEST_BOTH_EDGES;
	strcpy(p_event_request->consumer_label, "line consumer");
	p_event_request->fd = 0x65;

	test_addr(ARG_STR(GPIO_GET_LINEEVENT_IOCTL), "request", false, NULL);
	test_addr(ARG_STR(GPIO_GET_LINEEVENT_IOCTL), "request", false,
		  (char *) p_event_request + 1);

	ioctl_call(ARG_STR(GPIO_GET_LINEEVENT_IOCTL), NULL, p_event_request);
	argp_open("request", false);
	jsonl_struct_open();
	field_unsigned("lineoffset", true, p_event_request->lineoffset);
	field_handle_flags("handleflags", false, p_event_request->handleflags);
	jsonl_field_open("eventflags", false);
	jsonl_flags(p_event_request->eventflags, event_flags,
		    ARRAY_SIZE(event_flags), "GPIOEVENT_REQUEST_???");
	jsonl_field_close();
	field_string("consumer_label", false, p_event_request->consumer_label);
	jsonl_struct_close();
	argp_close();
	ioctl_end();
}

static void
test_gpiohandle_get_values(void)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(struct gpiohandle_data, p_handle_data);

	for (int i = 0; i < GPIOHANDLES_MAX; i++)
		p_handle_data->values[i] = i + 1;

	test_addr(ARG_STR(GPIOHANDLE_GET_LINE_VALUES_IOCTL), "values", true,
		  NULL);
	test_addr(ARG_STR(GPIOHANDLE_GET_LINE_VALUES_IOCTL), "values", true,
		  (char *) p_handle_data + 1);
	test_addr(ARG_STR(GPIOHANDLE_GET_LINE_VALUES_IOCTL), "values", true,
		  p_handle_data);
}

static void
test_gpiohandle_set_values(void)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(struct gpiohandle_data, p_handle_data);

	for (int i = 0; i < GPIOHANDLES_MAX; i++)
		p_handle_data->values[i] = i + 1;

	test_addr(ARG_STR(GPIOHANDLE_SET_LINE_VALUES_IOCTL), "values", false,
		  NULL);
	test_addr(ARG_STR(GPIOHANDLE_SET_LINE_VALUES_IOCTL), "values", false,
		  (char *) p_handle_data + 1);

	ioctl_call(ARG_STR(GPIOHANDLE_SET_LINE_VALUES_IOCTL), NULL,
		   p_handle_data);
	argp_open("values", false);
	jsonl_struct_open();
	FIELD_ARRAY("values", true, p_handle_data->values, GPIOHANDLES_MAX,
		    false);
	jsonl_struct_close();
	argp_close();
	ioctl_end();
}

static void
test_handle_config(const struct gpiohandle_config *config)
{
	ioctl_call(ARG_STR(GPIOHANDLE_SET_CONFIG_IOCTL), NULL, config);
	argp_open("config", false);
	jsonl_struct_open();
	field_handle_flags("flags", true, config->flags);
	FIELD_ARRAY("default_values", false, config->default_values,
		    GPIOHANDLES_MAX, false);
	jsonl_struct_close();
	argp_close();
	ioctl_end();
}

static void
test_gpiohandle_set_config(void)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(struct gpiohandle_config, p_handle_config);

	p_handle_config->flags =
		GPIOHANDLE_REQUEST_ACTIVE_LOW|GPIOHANDLE_REQUEST_OPEN_SOURCE;
	for (int i = 0; i < GPIOHANDLES_MAX; i++)
		p_handle_config->default_values[i] = i + 1;

	test_addr(ARG_STR(GPIOHANDLE_SET_CONFIG_IOCTL), "config", false, NULL);
	test_addr(ARG_STR(GPIOHANDLE_SET_CONFIG_IOCTL), "config", false,
		  (char *) p_handle_config + 1);

	test_handle_config(p_handle_config);

	p_handle_config->flags = UNK_GPIO_FLAG;
	test_handle_config(p_handle_config);
}

static void
test_v2_lineinfo(unsigned int code, const char *cmd,
		 const struct gpio_v2_line_info *info)
{
	ioctl_call(code, cmd, NULL, info);
	argp_open("info", false);
	jsonl_struct_open();
	field_unsigned("offset", true, info->offset);
	jsonl_struct_close();
	argp_close();
	ioctl_end();
}

static void
test_gpio_v2_line_info(void)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(struct gpio_v2_line_info, p_lineinfo);

	strcpy(p_lineinfo->name, "line name");
	strcpy(p_lineinfo->consumer, "line consumer");
	p_lineinfo->offset = 0x32;
	p_lineinfo->num_attrs = 0;
	p_lineinfo->flags =
		GPIO_V2_LINE_FLAG_ACTIVE_LOW|GPIO_V2_LINE_FLAG_BIAS_PULL_UP;
	memset(p_lineinfo->padding, 0, sizeof(p_lineinfo->padding));

	test_addr(ARG_STR(GPIO_V2_GET_LINEINFO_IOCTL), "info", false, NULL);
	test_addr(ARG_STR(GPIO_V2_GET_LINEINFO_IOCTL), "info", false,
		  (char *) p_lineinfo + 1);

	test_v2_lineinfo(ARG_STR(GPIO_V2_GET_LINEINFO_IOCTL), p_lineinfo);
	test_v2_lineinfo(ARG_STR(GPIO_V2_GET_LINEINFO_WATCH_IOCTL), p_lineinfo);

	for (unsigned int offset = 0x35; offset <= 0x39; ++offset) {
		p_lineinfo->offset = offset;
		test_v2_lineinfo(ARG_STR(GPIO_V2_GET_LINEINFO_IOCTL),
				 p_lineinfo);
	}
}

static void
field_line_config(const struct gpio_v2_line_config *config)
{
	jsonl_field_open("config", false);
	jsonl_struct_open();
	field_line_flags(true, config->flags);
	field_unsigned("num_attrs", false, config->num_attrs);
	jsonl_struct_close();
	jsonl_field_close();
}

static void
test_line_request(const struct gpio_v2_line_request *req)
{
	ioctl_call(ARG_STR(GPIO_V2_GET_LINE_IOCTL), NULL, req);
	argp_open("request", false);
	jsonl_struct_open();
	field_unsigned("num_lines", true, req->num_lines);
	FIELD_ARRAY("offsets", false, req->offsets,
		    MIN(req->num_lines, GPIO_V2_LINES_MAX), false);
	field_string("consumer", false, req->consumer);
	field_line_config(&req->config);
	if (req->event_buffer_size)
		field_unsigned("event_buffer_size", false,
			       req->event_buffer_size);
	if (req->padding[1])
		FIELD_ARRAY("padding", false, req->padding,
			    ARRAY_SIZE(req->padding), true);
	jsonl_struct_close();
	argp_close();
	ioctl_end();
}

static void
test_gpio_v2_line_request(void)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(struct gpio_v2_line_request,
				    p_line_request);

	p_line_request->offsets[0] = 0x12;
	p_line_request->offsets[1] = 0x23;
	p_line_request->offsets[2] = 0x34;
	p_line_request->offsets[3] = 0x45;
	strcpy(p_line_request->consumer, "line consumer");
	memset(&p_line_request->config, 0, sizeof(p_line_request->config));
	p_line_request->num_lines = 4;
	p_line_request->event_buffer_size = 0;
	memset(p_line_request->padding, 0, sizeof(p_line_request->padding));
	p_line_request->fd = 0x64;

	test_addr(ARG_STR(GPIO_V2_GET_LINE_IOCTL), "request", false, NULL);
	test_addr(ARG_STR(GPIO_V2_GET_LINE_IOCTL), "request", false,
		  (char *) p_line_request + 1);

	test_line_request(p_line_request);

	p_line_request->event_buffer_size = 0xdeafdace;
	test_line_request(p_line_request);
	p_line_request->event_buffer_size = 0;

	p_line_request->padding[1] = 0xfeedface;
	test_line_request(p_line_request);
	p_line_request->padding[1] = 0;

	/* num_lines > GPIO_V2_LINES_MAX */
	p_line_request->num_lines = GPIO_V2_LINES_MAX + 1;
	for (int i = 0; i < GPIO_V2_LINES_MAX; i++)
		p_line_request->offsets[i] = i + 1;
	test_line_request(p_line_request);
}

static void
test_gpio_v2_line_get_values(void)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(struct gpio_v2_line_values, p_line_values);

	p_line_values->bits = 0xcacafeedfacecafeULL;
	p_line_values->mask = 0xfadebeaddeedbabeULL;

	test_addr(ARG_STR(GPIO_V2_LINE_GET_VALUES_IOCTL), "values", false,
		  NULL);
	test_addr(ARG_STR(GPIO_V2_LINE_GET_VALUES_IOCTL), "values", false,
		  (char *) p_line_values + 1);

	ioctl_call(ARG_STR(GPIO_V2_LINE_GET_VALUES_IOCTL), NULL,
		   p_line_values);
	argp_open("values", false);
	jsonl_struct_open();
	field_hex("mask", true, p_line_values->mask);
	jsonl_struct_close();
	argp_close();
	ioctl_end();
}

static void
test_gpio_v2_line_set_values(void)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(struct gpio_v2_line_values, p_line_values);

	p_line_values->bits = 0xcacafeedfacecafeULL;
	p_line_values->mask = 0xfadebeaddeedbabeULL;

	test_addr(ARG_STR(GPIO_V2_LINE_SET_VALUES_IOCTL), "values", false,
		  NULL);
	test_addr(ARG_STR(GPIO_V2_LINE_SET_VALUES_IOCTL), "values", false,
		  (char *) p_line_values + 1);

	ioctl_call(ARG_STR(GPIO_V2_LINE_SET_VALUES_IOCTL), NULL,
		   p_line_values);
	argp_open("values", false);
	jsonl_struct_open();
	field_hex("bits", true, p_line_values->bits);
	field_hex("mask", false, p_line_values->mask);
	jsonl_struct_close();
	argp_close();
	ioctl_end();
}

/*
 * A flags attribute is printed by its flags unless its padding is set,
 * in which case the attribute is printed in its raw form.
 */
static void
emit_config_attr(const struct gpio_v2_line_config_attribute *attr)
{
	jsonl_struct_open();
	if (attr->attr.padding) {
		jsonl_field_open("attr", true);
		jsonl_struct_open();
		field_unsigned("id", true, attr->attr.id);
		field_hex("padding", false, attr->attr.padding);
		field_hex("data", false, attr->attr.values);
		jsonl_struct_close();
		jsonl_field_close();
	} else {
		field_line_flags(true, attr->attr.flags);
	}
	field_hex("mask", false, attr->mask);
	jsonl_struct_close();
}

static void
test_line_config(const struct gpio_v2_line_config *config)
{
	const unsigned int count =
		MIN(config->num_attrs, GPIO_V2_LINE_NUM_ATTRS_MAX);

	ioctl_call(ARG_STR(GPIO_V2_LINE_SET_CONFIG_IOCTL), NULL, config);
	argp_open("config", false);
	jsonl_struct_open();
	field_line_flags(true, config->flags);
	field_unsigned("num_attrs", false, config->num_attrs);
	if (count) {
		jsonl_field_open("attrs", false);
		jsonl_array_open();
		for (unsigned int i = 0; i < count; ++i) {
			printf("%s{", i ? ", " : "");
			emit_config_attr(&config->attrs[i]);
			printf("}");
		}
		jsonl_array_close();
		jsonl_field_close();
	}
	if (config->padding[1])
		FIELD_ARRAY("padding", false, config->padding,
			    ARRAY_SIZE(config->padding), true);
	jsonl_struct_close();
	argp_close();
	ioctl_end();
}

static void
test_gpio_v2_line_set_config(void)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(struct gpio_v2_line_config, p_line_config);

	p_line_config->flags =
		GPIO_V2_LINE_FLAG_ACTIVE_LOW|GPIO_V2_LINE_FLAG_BIAS_PULL_UP;
	p_line_config->num_attrs = 0;
	memset(p_line_config->attrs, 0, sizeof(p_line_config->attrs));
	memset(p_line_config->padding, 0, sizeof(p_line_config->padding));

	test_addr(ARG_STR(GPIO_V2_LINE_SET_CONFIG_IOCTL), "config", false,
		  NULL);
	test_addr(ARG_STR(GPIO_V2_LINE_SET_CONFIG_IOCTL), "config", false,
		  (char *) p_line_config + 1);

	test_line_config(p_line_config);

	p_line_config->flags = UNK_GPIO_FLAG;
	test_line_config(p_line_config);
	p_line_config->flags =
		GPIO_V2_LINE_FLAG_ACTIVE_LOW|GPIO_V2_LINE_FLAG_BIAS_PULL_UP;

	p_line_config->padding[1] = 0xfeedface;
	test_line_config(p_line_config);
	p_line_config->padding[1] = 0;

	/* num_attrs > GPIO_V2_LINE_NUM_ATTRS_MAX */
	for (int i = 0; i < GPIO_V2_LINE_NUM_ATTRS_MAX; i++) {
		p_line_config->attrs[i].mask = 2 * i + 1;
		p_line_config->attrs[i].attr.id = GPIO_V2_LINE_ATTR_ID_FLAGS;
		p_line_config->attrs[i].attr.flags =
			GPIO_V2_LINE_FLAG_ACTIVE_LOW
			| GPIO_V2_LINE_FLAG_BIAS_PULL_UP;
	}
	p_line_config->num_attrs = GPIO_V2_LINE_NUM_ATTRS_MAX + 1;
	test_line_config(p_line_config);

	p_line_config->num_attrs = 1;
	test_line_config(p_line_config);

	p_line_config->attrs[0].attr.padding = 0xfeedface;
	p_line_config->attrs[0].attr.values = 0xdeadbeefba11c0daULL;
	test_line_config(p_line_config);
}

int
main(int argc, char *argv[])
{
	merged = argc > 1 && !strcmp(argv[1], "merged");

	const unsigned long unknown_gpio_cmd =
		_IOC(_IOC_READ|_IOC_WRITE, 0xb4, 0x5e, 0xfed) |
		(unsigned long) 0xfacefeed00000000ULL;
	const unsigned long cmd_arg = (unsigned long) 0xdeadbeefbadc0dedULL;

	ioctl_call(unknown_gpio_cmd, NULL, NULL, (const void *) cmd_arg);
	argp_open("argp", false);
	jsonl_hex(cmd_arg);
	argp_close();
	ioctl_end();

	test_gpiochip_info();
	test_gpioline_info();
	test_gpioline_info_unwatch();
	test_gpiohandle_request();
	test_gpioevent_request();
	test_gpiohandle_get_values();
	test_gpiohandle_set_values();
	test_gpiohandle_set_config();

	test_gpio_v2_line_info();
	test_gpio_v2_line_request();
	test_gpio_v2_line_get_values();
	test_gpio_v2_line_set_values();
	test_gpio_v2_line_set_config();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl")

#endif
