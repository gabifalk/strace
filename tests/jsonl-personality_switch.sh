#!/bin/sh
#
# Common body for the generated jsonl-<fmt>-personality_switch tests: like
# jsonl.sh, but skipped when PTRACE_GET_SYSCALL_INFO does not work.
#
# Copyright (c) 2026 The strace developers.
# All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0-or-later

. "${srcdir=.}/init.sh"

$STRACE -d -enone / > /dev/null 2> "$LOG" ||:
grep -x "[^:]*strace: PTRACE_GET_SYSCALL_INFO works" "$LOG" > /dev/null ||
	skip_ 'PTRACE_GET_SYSCALL_INFO does not work'

. "${srcdir=.}/jsonl.sh"
