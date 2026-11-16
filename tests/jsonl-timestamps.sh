#!/bin/sh
#
# Common body for the generated jsonl-<fmt>-timestamps tests: like jsonl.sh,
# but the seconds and nanoseconds of time values are replaced with N.
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
sed -E -n '2,$ s/"(seconds|nanoseconds)": "[0-9]+"/"\1": "N"/g; 2,$p' \
	< "$LOG" > "$OUT"
match_diff "$OUT" "$EXP"
