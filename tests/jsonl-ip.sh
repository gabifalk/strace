#!/bin/sh
#
# Common body for the generated jsonl-<fmt>-ip tests: like jsonl.sh,
# but the hexadecimal instruction pointers are replaced with IP.
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

run_prog "$prog" "$fmt" > /dev/null
run_strace $jflag "$@" "$prog" "$fmt" > "$EXP"
sed -E -n '2,$ s/"ip": "([0-9a-f]{8}){1,2}"/"ip": "IP"/; 2,$p' \
	< "$LOG" > "$OUT"
match_diff "$OUT" "$EXP"
