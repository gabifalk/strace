#!/bin/sh
#
# Common body for the generated jsonl-<fmt>-summary tests: like jsonl.sh,
# but the time values of summary rows are replaced with N.
#
# Copyright (c) 2026 The strace developers.
# All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0-or-later

. "${srcdir=.}/init.sh"

rest="${NAME#jsonl-}"
fmt="${rest%%-*}"
prog="../jsonl-${rest#*-}"

case "$fmt" in
	split)	jflag=-J ;;
	merged)	jflag=--jsonl=merged ;;
	*)	fail_ "unknown jsonl format: $fmt" ;;
esac

$STRACE -d -enone / > /dev/null 2> "$LOG" ||:
grep -x "[^:]*strace: PTRACE_GET_SYSCALL_INFO works" "$LOG" > /dev/null ||
	skip_ 'PTRACE_GET_SYSCALL_INFO does not work'

run_prog "$prog" > "$EXP"
run_strace $jflag "$@" "$prog" > /dev/null
sed -E -n '2,$ {
	s/"(total|avg|min|max)_time": "[0-9.]+"/"\1_time": "N"/g
	/"event": "summary",/ s/"time_percent": "[0-9.]+"/"time_percent": "N"/
	p
}' < "$LOG" > "$OUT"
match_diff "$OUT" "$EXP"
