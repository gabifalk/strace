/*
 * Check JSONL output of NETLINK_KOBJECT_UEVENT messages.
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

# include <arpa/inet.h>
# include "netlink_kobject_uevent.h"

/* The first DEFAULT_STRLEN bytes out of len readable bytes of a string. */
static void
string(const void *buf, unsigned int len)
{
	jsonl_string_value(buf, MIN(len, DEFAULT_STRLEN),
			   len > DEFAULT_STRLEN);
}

# define FIELD_BE32(name_)						\
	do {								\
		jsonl_field_open(#name_, false);			\
		jsonl_hex(ntohl(uh->name_));				\
		jsonl_byteorder_big(sizeof(uh->name_));			\
		jsonl_field_close();					\
	} while (0)

# define FIELD_UINT(name_)						\
	do {								\
		jsonl_field_open(#name_, false);			\
		jsonl_unsigned(uh->name_);				\
		jsonl_field_close();					\
	} while (0)

static void
udev_header(const struct udev_monitor_netlink_header *uh)
{
	jsonl_struct_open();
	jsonl_field_open("prefix", true);
	string(uh->prefix, strlen(uh->prefix));
	jsonl_field_close();
	FIELD_BE32(magic);
	FIELD_UINT(header_size);
	FIELD_UINT(properties_off);
	FIELD_UINT(properties_len);
	FIELD_BE32(filter_subsystem_hash);
	FIELD_BE32(filter_devtype_hash);
	FIELD_BE32(filter_tag_bloom_hi);
	FIELD_BE32(filter_tag_bloom_lo);
	jsonl_struct_close();
}

static void
test_nlmsg_type_udev(const int fd)
{
	static const char extra[] = "12345678";
	struct udev_monitor_netlink_header uh = {
		.prefix = "libudev",
		.magic = htonl(0xfeedcafe),
		.header_size = sizeof(uh),
		.properties_off = 40,
		.properties_len = 299,
		.filter_subsystem_hash = htonl(0xc370b302),
		.filter_devtype_hash = htonl(0x10800000),
		.filter_tag_bloom_hi = htonl(0x2000400),
		.filter_tag_bloom_lo = htonl(0x10800000),
	};
	const unsigned int extra_len = LENGTH_OF(extra);
	const unsigned int uh_len = sizeof(uh);

	char *const buf = tail_alloc(uh_len + extra_len);
	memcpy(buf + extra_len, &uh, uh_len);

	jsonl_nl_sendto(fd, buf + extra_len, uh_len);
	jsonl_nl_sendto_open(fd);
	udev_header(&uh);
	jsonl_nl_sendto_close();

	memcpy(buf, &uh, uh_len);
	memcpy(buf + uh_len, extra, extra_len);
	jsonl_nl_sendto(fd, buf, uh_len + extra_len);
	jsonl_nl_sendto_open(fd);
	jsonl_array_open();
	printf("{");
	udev_header(&uh);
	printf("}, {");
	string(extra, extra_len);
	printf("}");
	jsonl_array_close();
	jsonl_nl_sendto_close();

	/* The header is one byte short of being readable. */
	memcpy(buf + extra_len + 1, &uh, uh_len - 1);
	jsonl_nl_sendto(fd, buf + extra_len + 1, uh_len);
	jsonl_nl_sendto_open(fd);
	string(&uh, uh_len - 1);
	jsonl_nl_sendto_close();
}

static void
test_nlmsg_type_kernel(const int fd)
{
	struct udev_monitor_netlink_header uh = {
		.prefix = "change@",
		.magic = htonl(0xfeedcafe),
		.header_size = sizeof(uh),
		.properties_off = 10,
		.properties_len = 299,
		.filter_subsystem_hash = htonl(0xfffffff),
		.filter_devtype_hash = htonl(0x10000000),
		.filter_tag_bloom_hi = htonl(0x2000400),
	};
	const unsigned int uh_len = sizeof(uh);

	TAIL_ALLOC_OBJECT_CONST_PTR(struct udev_monitor_netlink_header, p);
	memcpy(p, &uh, uh_len);

	jsonl_nl_sendto(fd, p, uh_len);
	jsonl_nl_sendto_open(fd);
	string(&uh, uh_len);
	jsonl_nl_sendto_close();
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	int fd = create_nl_socket(NETLINK_KOBJECT_UEVENT);

	test_nlmsg_type_udev(fd);
	test_nlmsg_type_kernel(fd);

	/* Data that looks like a zero-length C string. */
	char *const buf = tail_alloc(DEFAULT_STRLEN + 1);
	buf[0] = '=';
	fill_memory_ex(buf + 1, DEFAULT_STRLEN, 0, DEFAULT_STRLEN);

	jsonl_nl_sendto(fd, buf + 1, DEFAULT_STRLEN);
	jsonl_nl_sendto_open(fd);
	string(buf + 1, DEFAULT_STRLEN);
	jsonl_nl_sendto_close();

	jsonl_nl_sendto(fd, buf, DEFAULT_STRLEN + 1);
	jsonl_nl_sendto_open(fd);
	string(buf, DEFAULT_STRLEN + 1);
	jsonl_nl_sendto_close();

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_sendto")

#endif
