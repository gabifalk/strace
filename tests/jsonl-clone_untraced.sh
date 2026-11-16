#!/bin/sh
#
# Common body for the generated jsonl-<fmt>-clone_untraced tests: like
# jsonl.sh, but skipped when the seccomp filter is unavailable.
#
# Copyright (c) 2026 The strace developers.
# All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0-or-later

. "${srcdir=.}/init.sh"
. "${srcdir=.}/filter_seccomp.sh"
. "${srcdir=.}/jsonl.sh"
