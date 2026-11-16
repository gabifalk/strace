#!/bin/sh
#
# Common body for the generated jsonl-<fmt>-stack tests: like jsonl.sh,
# but only the frames of the functions of the test program are kept in
# stack traces, and their offsets and addresses are replaced with OFF
# and ADDR.
#
# Copyright (c) 2026 The strace developers.
# All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0-or-later

. "${srcdir=.}/init.sh"

$STRACE -V | grep '^Optional features enabled:.* stack-trace=' > /dev/null ||
	skip_ 'stack tracing is not enabled'

[ -f /proc/self/maps ] ||
	framework_skip_ '/proc/self/maps is not available'

rest="${NAME#jsonl-}"
fmt="${rest%%-*}"
prog="../jsonl-${rest#*-}"

case "$fmt" in
	split)	jflag=-J ;;
	merged)	jflag=--jsonl=merged ;;
	*)	fail_ "unknown jsonl format: $fmt" ;;
esac

run_prog "$prog" "$fmt" > /dev/null
run_strace $jflag "$@" "$prog" "$fmt" > "$EXP"
sed -E -n '2,$ {
	s/[{]"binary": ("[^"]*", "symbol": "(f[0-3]|main)", )/{"BINARY": \1/g
	s/[{]"binary": "[^"]*"[^}]*[}](, )?//g
	s/, \]/]/g
	s/"BINARY"/"binary"/g
	s/"offset": "0x[0-9a-f]+"/"offset": "OFF"/g
	s/"address": "0x[0-9a-f]+"/"address": "ADDR"/g
	p
}' < "$LOG" > "$OUT"

cmp -s "$OUT" "$EXP" ||
	case "$STRACE_ARCH" in
		aarch64|i386|ppc*|s390*|sparc*|x32|x86*) ;;
		*) skip_ "stack tracing is not supported on $STRACE_ARCH" ;;
	esac

match_diff "$OUT" "$EXP"
