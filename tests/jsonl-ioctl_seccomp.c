/*
 * Check JSONL output of the nr field of struct seccomp_data printed by
 * the SECCOMP_IOCTL_NOTIF_RECV ioctl command: a syscall value carrying
 * the personality the architecture and the number select, if any.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#if defined __NR_ioctl && defined __x86_64__ && !defined __ILP32__

# include "jsonl_ioctl.h"

# include <stdio.h>
# include <string.h>
# include <linux/audit.h>
# include <linux/seccomp.h>

static void
test_notif_recv(struct seccomp_notif *const notif, unsigned int nr,
		const char *name, unsigned int arch, const char *arch_sym,
		int pers)
{
	notif->data.nr = nr;
	notif->data.arch = arch;

	jsonl_ioctl_call(SECCOMP_IOCTL_NOTIF_RECV, "SECCOMP_IOCTL_NOTIF_RECV",
			 NULL, notif);
	jsonl_ioctl_argp_open("argp", false);
	jsonl_struct_open();
	jsonl_field_hex("id", true, 0);
	jsonl_field_pid("pid", false, "tgid", 0);
	jsonl_field_hex("flags", false, 0);
	printf(", \"data\": {");
	jsonl_struct_open();
	printf("\"nr\": {\"type\": \"syscall\", ");
	if (name)
		printf("\"name\": \"%s\", ", name);
	printf("\"scno\": \"%u\"", nr);
	if (pers >= 0)
		printf(", \"personality\": %d", pers);
	printf("}");
	jsonl_field_const("arch", false, arch, arch_sym,
			  arch_sym ? NULL : "AUDIT_ARCH_???");
	printf(", \"instruction_pointer\": {");
	jsonl_addr(NULL);
	printf("}, \"args\": {");
	jsonl_array_open();
	for (unsigned int i = 0; i < 6; ++i) {
		printf("%s{", i ? ", " : "");
		jsonl_hex(0);
		printf("}");
	}
	jsonl_array_close();
	printf("}");
	jsonl_struct_close();
	printf("}");
	jsonl_struct_close();
	if (jsonl_ioctl_merged) {
		printf("}, \"exit\": {");
	} else {
		printf("}]}\n");
		jsonl_syscall_open("ioctl", __NR_ioctl, false);
		jsonl_arg_open(2, "argp", NULL);
	}
	jsonl_addr(notif);
	jsonl_ioctl_exiting = true;
	jsonl_ioctl_argp_close();
	jsonl_ioctl_end();
}

int
main(int argc, char *argv[])
{
	jsonl_ioctl_merged = argc > 1 && !strcmp(argv[1], "merged");

	TAIL_ALLOC_OBJECT_CONST_PTR(struct seccomp_notif, notif);
	memset(notif, 0, sizeof(*notif));

	test_notif_recv(notif, 224, "gettid",
			AUDIT_ARCH_I386, "AUDIT_ARCH_I386", 1);
	test_notif_recv(notif, 186, "gettid",
			AUDIT_ARCH_X86_64, "AUDIT_ARCH_X86_64", 0);
	test_notif_recv(notif, 0x40000000 | 186, "gettid",
			AUDIT_ARCH_X86_64, "AUDIT_ARCH_X86_64", 2);
	test_notif_recv(notif, 0xbad, NULL,
			AUDIT_ARCH_X86_64, "AUDIT_ARCH_X86_64", 0);
	test_notif_recv(notif, 186, NULL, 0, NULL, -1);

	jsonl_exit_event(0);
	return 0;
}

#else

SKIP_MAIN_UNDEFINED("__NR_ioctl && __x86_64__ && !__ILP32__")

#endif
