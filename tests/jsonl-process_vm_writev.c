/*
 * Check JSONL output of the process_vm_writev syscall.
 *
 * Copyright (c) 2026 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tests.h"
#include "scno.h"

#define OP_STR "process_vm_writev"
#define OP_WR 1
#ifdef __NR_process_vm_writev
# define OP_NR __NR_process_vm_writev
#endif

#include "jsonl-process_vm_readv.c"
