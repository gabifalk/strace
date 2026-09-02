#!/bin/sh
#
# Common body for the generated jsonl-<fmt>-<syscall> tests.
#
# Copyright (c) 2026 The strace developers.
# All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0-or-later

. "${srcdir=.}/init.sh"

[ -n "$(get_config_option ENABLE_STRUCTURED_OUTPUT 1)" ] ||
	skip_ 'structured output is not compiled in'

rest="${NAME#jsonl-}"
fmt="${rest%%-*}"
prog="${rest#*-}-jsonl"

case "$fmt" in
	split)	jflag=-J ;;
	*)	fail_ "unknown jsonl format: $fmt" ;;
esac

run_strace_match_diff $jflag "$@" \
	QUIRK:PROG:"$prog" QUIRK:SKIP-LEADING-LINES:1
