/*
 * Check JSONL output of netlink protocol decoding.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined HAVE_SYS_XATTR_H && defined __NR_sendto

# include "jsonl_netlink.h"

# include <stdlib.h>
# include <sys/xattr.h>
# include <netinet/in.h>
# include <linux/sock_diag.h>
# include <linux/netlink_diag.h>
# include "xmalloc.h"

static const char *
nlmsg_type_sym(unsigned int type)
{
	switch (type) {
	case NLMSG_NOOP:
		return "NLMSG_NOOP";
	case NLMSG_ERROR:
		return "NLMSG_ERROR";
	case NLMSG_DONE:
		return "NLMSG_DONE";
	case SOCK_DIAG_BY_FAMILY:
		return "SOCK_DIAG_BY_FAMILY";
	}
	return NULL;
}

/*
 * The fields of a struct nlmsghdr value; p may be unaligned.
 * NLM_F_REQUEST and NLM_F_MULTI are known for every message type used here,
 * the ack flags for NLMSG_ERROR only.
 */
static void
nlh_fields(const void *p)
{
	static const struct strval32 flags[] = {
		{ ARG_STR(NLM_F_REQUEST) },
		{ ARG_STR(NLM_F_MULTI) },
		{ ARG_STR(NLM_F_CAPPED) },
		{ ARG_STR(NLM_F_ACK_TLVS) }
	};
	struct nlmsghdr nlh;

	memcpy(&nlh, p, sizeof(nlh));
	jsonl_nlmsghdr(p, nlmsg_type_sym(nlh.nlmsg_type), NULL, flags,
		       nlh.nlmsg_type == NLMSG_ERROR ? ARRAY_SIZE(flags) : 2);
}

/* A message with a payload, up to the fields of the payload value. */
static void
msg_begin(const void *nlh)
{
	jsonl_array_open();
	printf("{");
	nlh_fields(nlh);
	printf("}, {");
}

/* A struct nlmsgerr payload, up to the fields of its msg value. */
static void
err_begin(int error)
{
	printf("\"type\": \"struct\", \"fields\": {\"error\": "
	       "{\"type\": \"errno\", \"raw\": \"%d\", \"sym\": \"%s\"}, "
	       "\"msg\": {", error, errno2name(-error));
}

static void
err_end(void)
{
	printf("}}");
}

static void
send_query(const int fd)
{
	static const struct req {
		struct nlmsghdr nlh;
		const char magic[4];
	} c_req = {
		.nlh = {
			.nlmsg_len = sizeof(struct req),
			.nlmsg_type = NLMSG_NOOP,
			.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST
		},
		.magic = { 'a', 'b', 'c', 'd' }
	};
	struct req *const req = tail_memdup(&c_req, sizeof(c_req));

	/* zero address */
	jsonl_nl_sendto(fd, NULL, sizeof(*req));
	jsonl_nl_sendto_open(fd);
	jsonl_addr(NULL);
	jsonl_nl_sendto_close();

	/* zero length */
	jsonl_nl_sendto(fd, req, 0);
	jsonl_nl_sendto_open(fd);
	jsonl_bytes_value(req, 0, false);
	jsonl_nl_sendto_close();

	/* zero address and length */
	jsonl_nl_sendto(fd, NULL, 0);
	jsonl_nl_sendto_open(fd);
	jsonl_addr(NULL);
	jsonl_nl_sendto_close();

	/* unfetchable struct nlmsghdr */
	const void *const efault = tail_alloc(sizeof(struct nlmsghdr) - 1);
	jsonl_nl_sendto(fd, efault, sizeof(struct nlmsghdr));
	jsonl_nl_sendto_open(fd);
	jsonl_addr(efault);
	jsonl_nl_sendto_close();

	/* whole message length < sizeof(struct nlmsghdr) */
	jsonl_nl_sendto(fd, req->magic, sizeof(req->magic));
	jsonl_nl_sendto_open(fd);
	jsonl_bytes_value(req->magic, sizeof(req->magic), false);
	jsonl_nl_sendto_close();

	/* a single message with some data */
	jsonl_nl_sendto(fd, req, sizeof(*req));
	jsonl_nl_sendto_open(fd);
	msg_begin(&req->nlh);
	jsonl_bytes_value(req->magic, sizeof(req->magic), false);
	jsonl_nlmsg_close();
	jsonl_nl_sendto_close();

	/* a single message without data */
	req->nlh.nlmsg_len = sizeof(req->nlh);
	jsonl_nl_sendto(fd, &req->nlh, sizeof(req->nlh));
	jsonl_nl_sendto_open(fd);
	nlh_fields(&req->nlh);
	jsonl_nl_sendto_close();

	/* nlmsg_len > whole message length */
	req->nlh.nlmsg_len = sizeof(*req) + 8;
	jsonl_nl_sendto(fd, req, sizeof(*req));
	jsonl_nl_sendto_open(fd);
	msg_begin(&req->nlh);
	jsonl_bytes_value(req->magic, sizeof(req->magic), false);
	jsonl_nlmsg_close();
	jsonl_nl_sendto_close();

	/* nlmsg_len < sizeof(struct nlmsghdr) */
	req->nlh.nlmsg_len = 8;
	jsonl_nl_sendto(fd, req, sizeof(*req));
	jsonl_nl_sendto_open(fd);
	nlh_fields(&req->nlh);
	jsonl_nl_sendto_close();

	/* a sequence of two nlmsg objects */
	struct reqs {
		struct req req1;
		char padding[NLMSG_ALIGN(sizeof(struct req))
			     - sizeof(struct req)];
		struct req req2;
	};
	TAIL_ALLOC_OBJECT_CONST_PTR(struct reqs, reqs);
	memcpy(&reqs->req1, &c_req, sizeof(c_req));
	memcpy(&reqs->req2, &c_req, sizeof(c_req));

	jsonl_nl_sendto(fd, reqs, sizeof(*reqs));
	jsonl_nl_sendto_open(fd);
	jsonl_array_open();
	printf("{");
	msg_begin(&reqs->req1.nlh);
	jsonl_bytes_value(reqs->req1.magic, sizeof(reqs->req1.magic), false);
	jsonl_nlmsg_close();
	printf("}, {");
	msg_begin(&reqs->req2.nlh);
	jsonl_bytes_value(reqs->req2.magic, sizeof(reqs->req2.magic), false);
	jsonl_nlmsg_close();
	printf("}");
	jsonl_array_close();
	jsonl_nl_sendto_close();

	/* unfetchable second struct nlmsghdr */
	void *const efault2 = tail_memdup(&reqs->req1, sizeof(reqs->req1));
	jsonl_nl_sendto(fd, efault2, sizeof(*reqs));
	jsonl_nl_sendto_open(fd);
	jsonl_array_open();
	printf("{");
	msg_begin(&reqs->req1.nlh);
	jsonl_bytes_value(reqs->req1.magic, sizeof(reqs->req1.magic), false);
	jsonl_nlmsg_close();
	printf("}");
	jsonl_array_fetch_failed(&((struct reqs *) efault2)->req2);
	jsonl_nl_sendto_close();

	/* message length is not enough for the second struct nlmsghdr */
	jsonl_nl_sendto(fd, reqs, sizeof(*reqs) - sizeof(req->nlh));
	jsonl_nl_sendto_open(fd);
	jsonl_array_open();
	printf("{");
	msg_begin(&reqs->req1.nlh);
	jsonl_bytes_value(reqs->req1.magic, sizeof(reqs->req1.magic), false);
	jsonl_nlmsg_close();
	printf("}, {");
	jsonl_bytes_value(&reqs->req2.nlh,
			  sizeof(reqs->req2) - sizeof(req->nlh), false);
	printf("}");
	jsonl_array_close();
	jsonl_nl_sendto_close();

	/* second nlmsg_len < sizeof(struct nlmsghdr) */
	reqs->req2.nlh.nlmsg_len = 4;
	jsonl_nl_sendto(fd, reqs, sizeof(*reqs));
	jsonl_nl_sendto_open(fd);
	jsonl_array_open();
	printf("{");
	msg_begin(&reqs->req1.nlh);
	jsonl_bytes_value(reqs->req1.magic, sizeof(reqs->req1.magic), false);
	jsonl_nlmsg_close();
	printf("}, {");
	nlh_fields(&reqs->req2.nlh);
	printf("}");
	jsonl_array_close();
	jsonl_nl_sendto_close();

	/* abbreviated output */
# define ABBREV_LEN (DEFAULT_STRLEN + 1)
	const unsigned int msg_len = sizeof(struct nlmsghdr) * ABBREV_LEN;
	struct nlmsghdr *const msgs = tail_alloc(msg_len);
	for (unsigned int i = 0; i < ABBREV_LEN; ++i) {
		msgs[i].nlmsg_len = sizeof(*msgs);
		msgs[i].nlmsg_type = NLMSG_NOOP;
		msgs[i].nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
		msgs[i].nlmsg_seq = i;
		msgs[i].nlmsg_pid = 0;
	}

	jsonl_nl_sendto(fd, msgs, msg_len);
	jsonl_nl_sendto_open(fd);
	jsonl_array_open();
	for (unsigned int i = 0; i < DEFAULT_STRLEN; ++i) {
		printf("%s{", i ? ", " : "");
		nlh_fields(&msgs[i]);
		printf("}");
	}
	jsonl_array_close();
	jsonl_truncated();
	jsonl_nl_sendto_close();
}

static void
test_nlmsgerr_nesting(const int fd)
{
	enum {
		MAX_NESTING = 8,
		DEEP_NESTING = 0x4000,
		NESTING_STRIDE = NLMSG_HDRLEN + sizeof(int)
	};
	static const unsigned int depths[] = {
		1, MAX_NESTING - 1, MAX_NESTING,
		MAX_NESTING + 1, MAX_NESTING + 2, DEEP_NESTING
	};

	for (unsigned int i = 0; i < ARRAY_SIZE(depths); ++i) {
		const unsigned int depth = depths[i];
		const unsigned int len = depth * NESTING_STRIDE + NLMSG_HDRLEN;
		struct nlmsghdr *const nlh = xcalloc(1, len);
		const int error = -13;

		for (unsigned int j = 0; j < depth; ++j) {
			struct nlmsghdr *const err_nlh =
				(void *) nlh + j * NESTING_STRIDE;
			err_nlh->nlmsg_len = len - j * NESTING_STRIDE;
			err_nlh->nlmsg_type = NLMSG_ERROR;
			memcpy(NLMSG_DATA(err_nlh), &error, sizeof(error));
		}

		struct nlmsghdr *const terminal =
			(void *) nlh + depth * NESTING_STRIDE;
		terminal->nlmsg_len = NLMSG_HDRLEN;
		terminal->nlmsg_type = NLMSG_NOOP;

		jsonl_nl_sendto(fd, nlh, len);

		const unsigned int decoded = MIN(depth, MAX_NESTING);

		jsonl_nl_sendto_open(fd);
		for (unsigned int j = 0; j < decoded; ++j) {
			msg_begin((void *) nlh + j * NESTING_STRIDE);
			err_begin(error);
		}
		/* The payload of a message at the nesting limit is cut off. */
		if (depth > MAX_NESTING) {
			jsonl_array_open();
			printf("{");
		}
		nlh_fields((void *) nlh + decoded * NESTING_STRIDE);
		if (depth > MAX_NESTING) {
			printf("}");
			jsonl_array_close();
			jsonl_truncated();
		}
		for (unsigned int j = 0; j < decoded; ++j) {
			err_end();
			jsonl_nlmsg_close();
		}
		jsonl_nl_sendto_close();
		free(nlh);
	}
}

static void
test_nlmsgerr(const int fd)
{
	struct nlmsgerr *err;
	struct nlmsghdr *nlh;
	void *const nlh0 = midtail_alloc(NLMSG_HDRLEN, sizeof(*err) + 4);

	/* error message with nlmsg_len exceeding the allocated room */
	nlh = nlh0;
	nlh->nlmsg_len = NLMSG_HDRLEN + 4;
	nlh->nlmsg_type = NLMSG_ERROR;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_pid = 0;

	jsonl_nl_sendto(fd, nlh, nlh->nlmsg_len);
	jsonl_nl_sendto_open(fd);
	msg_begin(nlh);
	jsonl_addr(nlh0 + NLMSG_HDRLEN);
	jsonl_nlmsg_close();
	jsonl_nl_sendto_close();

	/* error message without enough room for the error code */
	nlh->nlmsg_len = NLMSG_HDRLEN + 2;
	nlh = nlh0 - 2;
	memmove(nlh, nlh0, sizeof(*nlh));
	memcpy(NLMSG_DATA(nlh), "42", 2);

	jsonl_nl_sendto(fd, nlh, NLMSG_HDRLEN + 2);
	jsonl_nl_sendto_open(fd);
	msg_begin(nlh);
	jsonl_bytes_value("42", 2, false);
	jsonl_nlmsg_close();
	jsonl_nl_sendto_close();

	/* error message with room for the error code only */
	nlh = nlh0 - sizeof(err->error);
	nlh->nlmsg_len = NLMSG_HDRLEN + sizeof(err->error);
	nlh->nlmsg_type = NLMSG_ERROR;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_pid = 0;
	err = NLMSG_DATA(nlh);
	fill_memory(&err->error, sizeof(err->error));

	jsonl_nl_sendto(fd, nlh, nlh->nlmsg_len);
	jsonl_nl_sendto_open(fd);
	msg_begin(nlh);
	jsonl_bytes_value(&err->error, sizeof(err->error), false);
	jsonl_nlmsg_close();
	jsonl_nl_sendto_close();

	/* error message without enough room for the whole struct nlmsgerr */
	nlh = nlh0 - (sizeof(*err) - 4);
	nlh->nlmsg_len = NLMSG_HDRLEN + (sizeof(*err) - 4);
	nlh->nlmsg_type = NLMSG_ERROR;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_pid = 0;
	err = NLMSG_DATA(nlh);
	fill_memory(err, sizeof(*err) - 4);

	jsonl_nl_sendto(fd, nlh, nlh->nlmsg_len);
	jsonl_nl_sendto_open(fd);
	msg_begin(nlh);
	jsonl_bytes_value(err, sizeof(*err) - 4, false);
	jsonl_nlmsg_close();
	jsonl_nl_sendto_close();

	/* error message with nlmsg_len exceeding the allocated room */
	nlh->nlmsg_len = NLMSG_HDRLEN + sizeof(*err);

	jsonl_nl_sendto(fd, nlh, nlh->nlmsg_len);
	jsonl_nl_sendto_open(fd);
	msg_begin(nlh);
	jsonl_addr(err);
	jsonl_nlmsg_close();
	jsonl_nl_sendto_close();

	/* error message with room for the error code and a header */
	nlh = nlh0 - sizeof(*err);
	nlh->nlmsg_len = NLMSG_HDRLEN + sizeof(*err);
	nlh->nlmsg_type = NLMSG_ERROR;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_pid = 0;
	err = NLMSG_DATA(nlh);
	err->error = -13;
	err->msg.nlmsg_len = NLMSG_HDRLEN;
	err->msg.nlmsg_type = NLMSG_NOOP;
	err->msg.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
	err->msg.nlmsg_seq = 42;
	err->msg.nlmsg_pid = 1234;

	static const unsigned int msg_lens[] = {
		NLMSG_HDRLEN,
		/* err->msg.nlmsg_len < sizeof(err->msg) */
		sizeof(err->msg.nlmsg_len),
		/* nlh->nlmsg_len < NLMSG_HDRLEN + err->msg.nlmsg_len */
		NLMSG_HDRLEN + 4
	};
	for (unsigned int i = 0; i < ARRAY_SIZE(msg_lens); ++i) {
		err->msg.nlmsg_len = msg_lens[i];

		jsonl_nl_sendto(fd, nlh, nlh->nlmsg_len);
		jsonl_nl_sendto_open(fd);
		msg_begin(nlh);
		err_begin(err->error);
		nlh_fields(&err->msg);
		err_end();
		jsonl_nlmsg_close();
		jsonl_nl_sendto_close();
	}

	/* error message with room for the error code, a header, and data */
	nlh = nlh0 - sizeof(*err) - 4;
	nlh->nlmsg_len = NLMSG_HDRLEN + sizeof(*err) + 4;
	nlh->nlmsg_type = NLMSG_ERROR;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_pid = 0;
	err = NLMSG_DATA(nlh);
	err->error = -13;
	err->msg.nlmsg_len = NLMSG_HDRLEN + 4;
	err->msg.nlmsg_type = NLMSG_NOOP;
	err->msg.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
	err->msg.nlmsg_seq = 421;
	err->msg.nlmsg_pid = 12345;
	memcpy(NLMSG_DATA(&err->msg), "abcd", 4);

	jsonl_nl_sendto(fd, nlh, nlh->nlmsg_len);
	jsonl_nl_sendto_open(fd);
	msg_begin(nlh);
	err_begin(err->error);
	msg_begin(&err->msg);
	jsonl_bytes_value("abcd", 4, false);
	jsonl_nlmsg_close();
	err_end();
	jsonl_nlmsg_close();
	jsonl_nl_sendto_close();

	nlh->nlmsg_len = NLMSG_HDRLEN + sizeof(*err) + 1;
	err->msg.nlmsg_len = NLMSG_HDRLEN + 1;
	err->msg.nlmsg_type = SOCK_DIAG_BY_FAMILY;
	err->msg.nlmsg_flags = NLM_F_REQUEST;
	err->msg.nlmsg_seq = 4213;
	err->msg.nlmsg_pid = 123456;
	memcpy(NLMSG_DATA(&err->msg), "", 1);

	jsonl_nl_sendto(fd, nlh, nlh->nlmsg_len);
	jsonl_nl_sendto_open(fd);
	msg_begin(nlh);
	err_begin(err->error);
	msg_begin(&err->msg);
	jsonl_struct_open();
	jsonl_field_open("family", true);
	jsonl_const(AF_UNSPEC, true, "AF_UNSPEC", NULL);
	jsonl_field_close();
	jsonl_struct_close();
	jsonl_nlmsg_close();
	err_end();
	jsonl_nlmsg_close();
	jsonl_nl_sendto_close();
}

static void
test_nlmsg_done(const int fd)
{
	struct nlmsghdr *nlh;
	const int num = 0xfacefeed;
	void *const nlh0 = midtail_alloc(NLMSG_HDRLEN, sizeof(num));

	/* NLMSG_DONE message without enough room for an integer payload */
	nlh = nlh0;
	*nlh = (struct nlmsghdr) {
		.nlmsg_len = NLMSG_HDRLEN + sizeof(num),
		.nlmsg_type = NLMSG_DONE,
		.nlmsg_flags = NLM_F_MULTI
	};

	jsonl_nl_sendto(fd, nlh, nlh->nlmsg_len);
	jsonl_nl_sendto_open(fd);
	msg_begin(nlh);
	jsonl_addr(nlh0 + NLMSG_HDRLEN);
	jsonl_nlmsg_close();
	jsonl_nl_sendto_close();

	/* NLMSG_DONE message with enough room for an oddly short payload */
	nlh->nlmsg_len = NLMSG_HDRLEN + 2;
	nlh = nlh0 - 2;
	/* Beware of unaligned access to nlh members. */
	memmove(nlh, nlh0, sizeof(*nlh));
	memcpy(NLMSG_DATA(nlh), "42", 2);

	jsonl_nl_sendto(fd, nlh, NLMSG_HDRLEN + 2);
	jsonl_nl_sendto_open(fd);
	msg_begin(nlh);
	jsonl_bytes_value("42", 2, false);
	jsonl_nlmsg_close();
	jsonl_nl_sendto_close();

	/* NLMSG_DONE message with enough room for an integer payload */
	nlh = nlh0 - sizeof(num);
	*nlh = (struct nlmsghdr) {
		.nlmsg_len = NLMSG_HDRLEN + sizeof(num),
		.nlmsg_type = NLMSG_DONE,
		.nlmsg_flags = NLM_F_MULTI
	};
	memcpy(NLMSG_DATA(nlh), &num, sizeof(num));

	jsonl_nl_sendto(fd, nlh, nlh->nlmsg_len);
	jsonl_nl_sendto_open(fd);
	msg_begin(nlh);
	jsonl_decimal(num);
	jsonl_nlmsg_close();
	jsonl_nl_sendto_close();
}

static void
test_ack_flags(const int fd)
{
	static const unsigned int flags[] = {
		NLM_F_REQUEST | NLM_F_CAPPED,
		NLM_F_REQUEST | NLM_F_ACK_TLVS,
		NLM_F_REQUEST | NLM_F_CAPPED | NLM_F_ACK_TLVS
	};
	struct nlmsghdr nlh = {
		.nlmsg_len = sizeof(nlh),
		.nlmsg_type = NLMSG_ERROR,
	};

	for (unsigned int i = 0; i < ARRAY_SIZE(flags); ++i) {
		nlh.nlmsg_flags = flags[i];
		jsonl_nl_sendto(fd, &nlh, sizeof(nlh));
		jsonl_nl_sendto_open(fd);
		nlh_fields(&nlh);
		jsonl_nl_sendto_close();
	}
}

int
main(int argc, char *argv[])
{
	jsonl_nl_merged = argc > 1 && !strcmp(argv[1], "merged");

	const int fd = create_nl_socket(NETLINK_SOCK_DIAG);

	char *path = xasprintf("/proc/self/fd/%u", fd);
	char buf[256];
	if (getxattr(path, "system.sockprotoname", buf, sizeof(buf) - 1) < 0)
		perror_msg_and_skip("getxattr");
	free(path);

	send_query(fd);
	test_nlmsgerr(fd);
	test_nlmsgerr_nesting(fd);
	test_nlmsg_done(fd);
	test_ack_flags(fd);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("HAVE_SYS_XATTR_H && __NR_sendto")

#endif
