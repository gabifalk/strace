#!/bin/sh
#
# Common body for the generated JSONL tests of ioctl decoding: like jsonl.sh,
# but ioctl calls on descriptors 0, 1, and 2 are filtered out, as in ioctl.test
# (musl calls ioctl(TIOCGWINSZ) on stdout before its first write).
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

check_prog awk
run_prog "$prog" "$fmt" > /dev/null
run_strace $jflag "$@" "$prog" "$fmt" > "$EXP"
awk '
NR == 1 { next }
skip { skip = 0; if (/"entering": false/) next }
/"name": "ioctl"/ &&
/"args": \[\{"index": 0, "arg": "fd", / &&
/"arg": "fd", ("entry": \{)?"type": "fd", "raw": "[012]"[,}]/ {
	skip = /"entering": true/
	next
}
{ print }
' < "$LOG" > "$OUT"
match_diff "$OUT" "$EXP"
