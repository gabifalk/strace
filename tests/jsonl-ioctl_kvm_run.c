/*
 * Check JSONL output of the vcpu_run field of KVM_RUN with -e kvm=vcpu+.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"

#if defined HAVE_LINUX_KVM_H				\
 && defined HAVE_STRUCT_KVM_REGS			\
 && defined HAVE_STRUCT_KVM_SREGS			\
 && defined HAVE_STRUCT_KVM_USERSPACE_MEMORY_REGION	\
 &&(defined __x86_64__ || defined __i386__)

# include "scno.h"
# include "jsonl.h"

# include <errno.h>
# include <fcntl.h>
# include <pthread.h>
# include <stdio.h>
# include <string.h>
# include <sys/ioctl.h>
# include <sys/mman.h>
# include <unistd.h>
# include <linux/kvm.h>

extern const char code[];
extern const unsigned short code_size;

__asm__(
	".type code, @object		\n"
	".code16			\n"
	"code:				\n"
	"	mov $0x03f8, %dx	\n"
	"	movb $'\n', %al		\n"
	"	out %al, (%dx)		\n"
	"	mov $0x2000, %dx	\n"
	"	movb $0xdf, (%edx)	\n"
	"	mov $0x2001, %dx	\n"
	"	movb (%edx), %al	\n"
	"	mov $0x03f9, %dx	\n"
	"	out %al, (%dx)		\n"
	"	hlt			\n"
# ifdef __x86_64__
	".code64			\n"
# else
	".code32			\n"
# endif
	".size code, . - code		\n"
	".type code_size, @object	\n"
	"code_size:			\n"
	"	.short . - code		\n"
	".size code_size, . - code_size	\n"
# ifdef __x86_64__
	".code64			\n"
# else
	".code32			\n"
# endif
	);

static int vcpu_fd;
static struct kvm_run *run;
static size_t mmap_size;

static int
kvm_ioctl(int fd, unsigned long cmd, const char *cmd_str, void *arg)
{
	int rc = ioctl(fd, cmd, arg);
	if (rc < 0)
		perror_msg_and_skip("%s", cmd_str);
	return rc;
}

# define KVM_IOCTL(fd_, cmd_, arg_)	\
	kvm_ioctl((fd_), (cmd_), #cmd_, (arg_))

static void *
setup(void *arg)
{
	static const char vcpu_dev[] = "anon_inode:kvm-vcpu:0";

	int kvm = open("/dev/kvm", O_RDWR);
	if (kvm < 0)
		perror_msg_and_skip("open: %s", "/dev/kvm");

	int ret = KVM_IOCTL(kvm, KVM_GET_API_VERSION, 0);
	if (ret != KVM_API_VERSION)
		error_msg_and_skip("KVM_GET_API_VERSION returned %d"
				   ", KVM_API_VERSION is %d",
				   ret, KVM_API_VERSION);

	int vm_fd = KVM_IOCTL(kvm, KVM_CREATE_VM, 0);

	const size_t page_size = get_page_size();
	void *const mem = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (mem == MAP_FAILED)
		perror_msg_and_fail("mmap page");

	struct kvm_userspace_memory_region region = {
		.slot = 0,
		.guest_phys_addr = page_size,
		.memory_size = page_size,
		.userspace_addr = (uintptr_t) mem,
	};
	KVM_IOCTL(vm_fd, KVM_SET_USER_MEMORY_REGION, &region);

	vcpu_fd = KVM_IOCTL(vm_fd, KVM_CREATE_VCPU, NULL);

	char path[sizeof("/proc/self/fd/") + sizeof(int) * 3];
	char buf[sizeof(vcpu_dev)];
	snprintf(path, sizeof(path), "/proc/self/fd/%d", vcpu_fd);
	if (readlink(path, buf, sizeof(buf)) != sizeof(buf) - 1
	    || memcmp(buf, vcpu_dev, sizeof(buf) - 1))
		error_msg_and_skip("newer kernel (>= 4.16) is needed");

	ret = KVM_IOCTL(kvm, KVM_GET_VCPU_MMAP_SIZE, NULL);
	if (ret < (int) sizeof(*run))
		error_msg_and_fail("KVM_GET_VCPU_MMAP_SIZE returned %d < %d",
				   ret, (int) sizeof(*run));
	mmap_size = (ret + page_size - 1) & -page_size;
	run = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED,
		   vcpu_fd, 0);
	if (run == MAP_FAILED)
		perror_msg_and_fail("mmap vcpu");

	struct kvm_sregs sregs;
	KVM_IOCTL(vcpu_fd, KVM_GET_SREGS, &sregs);
	sregs.cs.base = 0;
	sregs.cs.selector = 0;
	KVM_IOCTL(vcpu_fd, KVM_SET_SREGS, &sregs);

	struct kvm_regs regs = {
		.rip = page_size,
		.rax = 2,
		.rbx = 2,
		.rflags = 0x2,
	};
	KVM_IOCTL(vcpu_fd, KVM_SET_REGS, &regs);

	memcpy(mem, code, code_size);

	return NULL;
}

static const char *
exit_reason_sym(unsigned int reason)
{
	switch (reason) {
	case KVM_EXIT_UNKNOWN:	return "KVM_EXIT_UNKNOWN";
	case KVM_EXIT_IO:	return "KVM_EXIT_IO";
	case KVM_EXIT_HLT:	return "KVM_EXIT_HLT";
	case KVM_EXIT_MMIO:	return "KVM_EXIT_MMIO";
	default:		return NULL;
	}
}

static void
print_byte_array(const unsigned char *data, size_t len)
{
	jsonl_array_open();
	for (size_t i = 0; i < len; ++i) {
		printf("%s{", i ? ", " : "");
		jsonl_hex(data[i]);
		printf("}");
	}
	jsonl_array_close();
}

static void
print_kvm_run(const char *name, const struct kvm_run *r)
{
	const char *const sym = exit_reason_sym(r->exit_reason);

	printf(", \"%s\": {", name);
	jsonl_struct_open();
	jsonl_field_unsigned("request_interrupt_window", true,
			     r->request_interrupt_window);
# ifdef HAVE_STRUCT_KVM_RUN_IMMEDIATE_EXIT
	jsonl_field_unsigned("immediate_exit", false, r->immediate_exit);
# endif
	jsonl_field_open("exit_reason", false);
	jsonl_const(r->exit_reason, false, sym, sym ? NULL : "KVM_EXIT_???");
	jsonl_field_close();
	jsonl_field_unsigned("ready_for_interrupt_injection", false,
			     r->ready_for_interrupt_injection);
	jsonl_field_unsigned("if_flag", false, r->if_flag);
# ifdef HAVE_STRUCT_KVM_RUN_FLAGS
	jsonl_field_hex("flags", false, r->flags);
# endif
	jsonl_field_open("cr8", false);
	jsonl_hex(r->cr8);
	jsonl_field_close();
	jsonl_field_open("apic_base", false);
	jsonl_hex(r->apic_base);
	jsonl_field_close();

	switch (r->exit_reason) {
	case KVM_EXIT_IO:
		jsonl_field_open("io", false);
		jsonl_struct_open();
		jsonl_field_const("direction", true, r->io.direction,
				  r->io.direction == KVM_EXIT_IO_IN
				  ? "KVM_EXIT_IO_IN" : "KVM_EXIT_IO_OUT",
				  NULL);
		jsonl_field_unsigned("size", false, r->io.size);
		jsonl_field_open("port", false);
		jsonl_hex(r->io.port);
		jsonl_field_close();
		jsonl_field_unsigned("count", false, r->io.count);
		jsonl_field_open("data_offset", false);
		jsonl_hex(r->io.data_offset);
		jsonl_field_close();
		jsonl_field_open("data", false);
		print_byte_array((const unsigned char *) r + r->io.data_offset,
				 r->io.size * r->io.count);
		jsonl_field_close();
		jsonl_struct_close();
		jsonl_field_close();
		break;
	case KVM_EXIT_MMIO:
		jsonl_field_open("mmio", false);
		jsonl_struct_open();
		jsonl_field_open("phys_addr", true);
		jsonl_hex(r->mmio.phys_addr);
		jsonl_field_close();
		jsonl_field_open("data", false);
		print_byte_array(r->mmio.data, sizeof(r->mmio.data));
		jsonl_field_close();
		jsonl_field_unsigned("len", false, r->mmio.len);
		jsonl_field_unsigned("is_write", false, r->mmio.is_write);
		jsonl_struct_close();
		jsonl_field_close();
		break;
	}

	jsonl_struct_close();
	printf("}");
}

static void
print_kvm_run_ioctl(bool merged, const struct kvm_run *before,
		    const struct kvm_run *after)
{
	const char *const side = merged ? "entry" : NULL;

	if (merged)
		jsonl_syscall_merged_open("ioctl", __NR_ioctl);
	else
		jsonl_syscall_open("ioctl", __NR_ioctl, true);
	jsonl_arg_open(0, "fd", side);
	jsonl_fd(vcpu_fd, NULL, false);
	jsonl_arg_close(side);
	printf(", ");
	jsonl_arg_open(1, "op", side);
	printf("\"type\": \"ioctl_op\", \"raw\": \"%#x\", \"cmds\":"
	       " [{\"type\": \"ioctl_cmd\", \"name\": \"KVM_RUN\"}]",
	       (unsigned int) KVM_RUN);
	printf(", \"dir\": {");
	jsonl_flags_open(_IOC_NONE);
	jsonl_flags_const(_IOC_NONE, "_IOC_NONE", true);
	jsonl_flags_close();
	printf("}, \"ioc_type\": \"%#x\", \"ioc_nr\": \"%#x\","
	       " \"ioc_size\": \"%#x\"",
	       _IOC_TYPE(KVM_RUN), _IOC_NR(KVM_RUN), _IOC_SIZE(KVM_RUN));
	jsonl_arg_close(side);
	if (merged) {
		printf(", ");
	} else {
		printf("]}\n");
		jsonl_syscall_open("ioctl", __NR_ioctl, false);
	}
	jsonl_arg_open(2, "argp", merged ? "exit" : NULL);
	jsonl_hex(0);
	jsonl_arg_close(merged ? "exit" : NULL);

	const char *const sym = exit_reason_sym(after->exit_reason);
	printf("], \"return\": {\"type\": \"kvm_run_result\", \"raw\": \"0\","
	       " \"exit_reason\": {");
	jsonl_const(after->exit_reason, true, sym, sym ? NULL : "KVM_EXIT_???");
	printf("}}, \"vcpu_run\": {\"type\": \"kvm_vcpu_run\","
	       " \"vcpu\": \"0\"");
	print_kvm_run("before", before);
	print_kvm_run("after", after);
	printf("}}\n");
}

int
main(int argc, char *argv[])
{
	const bool merged = argc > 1 && !strcmp(argv[1], "merged");

	skip_if_unavailable("/proc/self/fd/");

	pthread_t t;
	errno = pthread_create(&t, NULL, setup, NULL);
	if (errno)
		perror_msg_and_fail("pthread_create");
	errno = pthread_join(t, NULL);
	if (errno)
		perror_msg_and_fail("pthread_join");

	struct kvm_run *const before = tail_alloc(mmap_size);
	struct kvm_run *const after = tail_alloc(mmap_size);

	for (;;) {
		memcpy(before, run, mmap_size);
		if (ioctl(vcpu_fd, KVM_RUN, NULL))
			perror_msg_and_fail("KVM_RUN");
		memcpy(after, run, mmap_size);
		print_kvm_run_ioctl(merged, before, after);

		switch (run->exit_reason) {
		case KVM_EXIT_HLT:
			jsonl_exit_event(0);
			return 0;
		case KVM_EXIT_IO:
			if (run->io.direction != KVM_EXIT_IO_OUT
			    || run->io.size != 1
			    || run->io.count != 1
			    || run->io.data_offset >= mmap_size
			    || (run->io.port != 0x03f8
				&& run->io.port != 0x03f9))
				error_msg_and_fail("unhandled KVM_EXIT_IO");
			break;
		case KVM_EXIT_MMIO:
			if (run->mmio.len != 1)
				error_msg_and_fail("unhandled KVM_EXIT_MMIO");
			if (!run->mmio.is_write)
				run->mmio.data[0] = 0xab;
			break;
		default:
			error_msg_and_fail("exit_reason = %#x",
					   run->exit_reason);
		}
	}
}

#else

SKIP_MAIN_UNDEFINED("HAVE_LINUX_KVM_H && HAVE_STRUCT_KVM_REGS && "
		    "HAVE_STRUCT_KVM_SREGS && "
		    "HAVE_STRUCT_KVM_USERSPACE_MEMORY_REGION && "
		    "(__x86_64__ || __i386__)")

#endif
