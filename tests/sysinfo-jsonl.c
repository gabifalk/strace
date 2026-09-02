/*
 * Check JSONL output of the sysinfo syscall entry and struct exit events.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "jsonl.h"
#include "scno.h"

#include <stdio.h>
#include <sys/sysinfo.h>
#include <unistd.h>

static void
jsonl_u(const char *name, unsigned long long val, bool last)
{
	printf("\"%s\": {\"type\": \"unsigned\", \"raw\": \"%llu\"}%s",
	       name, val, last ? "" : ", ");
}

/* The sysinfo "info" argument as strace decodes it. */
static void
jsonl_sysinfo_struct(const struct sysinfo *si)
{
	printf("{\"arg\": \"info\", \"type\": \"struct\", \"fields\": {");
	jsonl_u("uptime", (unsigned long) si->uptime, false);
	printf("\"loads\": {\"type\": \"array\", \"elems\": [");
	for (unsigned int i = 0; i < 3; i++)
		printf("{\"type\": \"unsigned\", \"raw\": \"%llu\"}%s",
		       (unsigned long long) si->loads[i], i < 2 ? ", " : "");
	printf("]}, ");
	jsonl_u("totalram", si->totalram, false);
	jsonl_u("freeram", si->freeram, false);
	jsonl_u("sharedram", si->sharedram, false);
	jsonl_u("bufferram", si->bufferram, false);
	jsonl_u("totalswap", si->totalswap, false);
	jsonl_u("freeswap", si->freeswap, false);
	jsonl_u("procs", si->procs, false);
	jsonl_u("totalhigh", si->totalhigh, false);
	jsonl_u("freehigh", si->freehigh, false);
	jsonl_u("mem_unit", si->mem_unit, true);
	printf("}}");
}

int
main(void)
{
	TAIL_ALLOC_OBJECT_CONST_PTR(struct sysinfo, si);

	if (syscall(__NR_sysinfo, si))
		perror_msg_and_skip("sysinfo");

	jsonl_syscall_open("sysinfo", __NR_sysinfo, true);
	printf("]}\n");
	jsonl_syscall_open("sysinfo", __NR_sysinfo, false);
	jsonl_sysinfo_struct(si);
	printf("], \"return\": {\"type\": \"unsigned\","
	       " \"raw\": \"0\"}}\n");
	return 0;
}
