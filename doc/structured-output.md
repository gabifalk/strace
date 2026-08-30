# strace Structured JSONL Output

*Draft -- describes proposed schema version 1.*

strace can emit its trace as a **JSONL** (JSON Lines) stream: one JSON object
per line, one line per event. This document describes the event stream, the
value types, and the options that shape them.

> **Status.** This is a **proposal for review**, aimed at strace developers.
> It specifies a format so we can evaluate the design *before* committing to
> implementing it. The current code is an experimental prototype, not the
> reference -- where prototype and document disagree, the document is the
> thing under discussion. Feedback on the design is the goal.
>
> Throughout, "consumer" means a downstream tool that parses the JSONL; the
> format exists to serve such consumers, and the `Traditional:` lines show how
> each value maps to classic strace output so reviewers can check the
> structured form is faithful.

## Contents

- [Overview](#overview)
  - [Format variants](#format-variants)
  - [About the JSON examples](#about-the-json-examples)
  - [Event types](#event-types)
- [Invocation](#invocation)
- [Design principles](#design-principles)
- [Document conventions](#document-conventions)
- [Syscall events](#syscall-events)
  - [Event shape](#event-shape)
  - [Event-level fields](#event-level-fields)
  - [In-out arguments and value changes](#in-out-arguments-and-value-changes)
  - [Split structs](#split-structs)
  - [Return, timing, and injection](#return-timing-and-injection)
- [Other events](#other-events)
  - [Header event](#header-event)
  - [Process and signal events](#process-and-signal-events)
  - [Summary events](#summary-events)
- [Value type reference](#value-type-reference)
  - [Simple value types](#simple-value-types)
  - [Meta-fields](#meta-fields)
  - [const, flags, and bitset](#const-flags-and-bitset)
  - [fd](#fd)
  - [mode](#mode)
  - [timespec_t and timeval_t](#timespec_t-and-timeval_t)
  - [time and duration](#time-and-duration)
  - [struct and array](#struct-and-array)
  - [sockaddr](#sockaddr)
  - [syscall](#syscall)
  - [wait_status](#wait_status)
  - [ioctl_op](#ioctl_op)
  - [uring_restriction_op](#uring_restriction_op)
  - [The return field](#the-return-field)
  - [Stack traces](#stack-traces)
- [The `style` display hint](#the-style-display-hint)
- [Options that affect output](#options-that-affect-output)
- [Worked example: sysinfo](#worked-example-sysinfo)
- [Appendix: non-normative notes](#appendix-non-normative-notes)

## Overview

Each line of output is one self-contained JSON object describing one event: a
syscall entry or exit, a signal, a process state change, or a summary row.

### Format variants

Two variants are available, selected with `-B`:

- **`-B jsonl-split`** (alias: `-B jsonl`) -- two events per syscall (one at
  entry, one at exit). This is the default JSONL mode.
- **`-B jsonl-merged`** -- one event per syscall, emitted at exit time, with
  each argument represented as an `[entry_value, exit_value]` pair. Entry
  arguments are buffered at syscall entry and combined with exit arguments at
  exit time.

The [header event](#header-event)'s `format` field names the active variant so
consumers know whether to expect separate entry/exit events or merged
`[entry, exit]` argument arrays.

**Limitations of `jsonl-merged`.** Because a merged syscall produces a single
event emitted only at exit time, information tied to the *moment of entry* is
lost:

- **No real-time entry visibility.** A long-running syscall (a blocking
  `read`, a `sleep`, etc.) produces no output until it returns. Traditional
  output and `jsonl-split` both surface the entry immediately.
- **No interleaving across pids/threads.** Concurrent syscalls in different
  threads appear in exit order rather than in the order they entered vs.
  returned. Traditional output's `<unfinished ...>` / `<... resumed>`
  mechanism and `jsonl-split`'s separate entry/exit events both preserve this
  ordering; `jsonl-merged` does not.

### About the JSON examples

In real output every event occupies exactly one line -- no embedded newlines,
no indentation. The JSON snippets here are reformatted for readability --
pretty-printed when nesting makes that clearer, kept on one line when short --
and each corresponds to a single physical line of strace's output. Within an
example, `...` marks fields elided for brevity.

### Event types

Every event has an `event` field naming its type, and a `pid`. The full set:

| `event` | Purpose | Section |
|---------|---------|---------|
| `header` | First line: schema version, format, options, capabilities | [Header event](#header-event) |
| `syscall` | A syscall (entry, exit, or merged) | [Syscall events](#syscall-events) |
| `signal` | A signal delivered with siginfo | [Process and signal events](#process-and-signal-events) |
| `stopped` | Stopped by a signal, no siginfo | [Process and signal events](#process-and-signal-events) |
| `killed` | Killed by a signal | [Process and signal events](#process-and-signal-events) |
| `exit` | Process exited | [Process and signal events](#process-and-signal-events) |
| `detached` | strace detached from the process | [Process and signal events](#process-and-signal-events) |
| `summary_header` | Personality header before summary rows (`-c`/`-C`) | [Summary events](#summary-events) |
| `summary` | Per-syscall summary row (`-c`/`-C`) | [Summary events](#summary-events) |
| `summary_total` | Aggregate summary totals (`-c`/`-C`) | [Summary events](#summary-events) |

## Invocation

```
strace -B jsonl-split   <command>     # two events per syscall
strace -B jsonl         <command>     # alias for the above
strace -B jsonl-merged  <command>     # one event per syscall
```

All other strace flags work as usual. The flags that control *what data*
strace collects (`-y`, `-yy`, `-k`, `-s`, `-e verbose=...`, etc.) determine
which optional fields the structured output carries -- see
[Options that affect output](#options-that-affect-output).

## Design principles

The decisions below shape the whole format; the rest of the document follows
from them. They are the first thing to review -- if a principle is wrong, much
of the format changes with it.

1. **Presentation only.** Structured output is a different rendering of the
   same data. It never implies extra data-fetching beyond what the user
   requested with flags like `-y`, `-yy`, `-k`.

2. **Always emit raw + symbolic.** For values with symbolic translations
   (constants, flags), both `raw` and `sym` are included whenever known,
   regardless of the `-X` setting -- both are already available internally at
   no extra cost.

3. **Structured decomposition.** Whenever strace has structured data
   internally, it emits structured fields, not formatted strings. Device info
   becomes `{"type": "dev", "kind": "char", "major": "1", "minor": "3"}`, not
   `"char 1:3"`.

4. **No unfinished/resumed.** In `jsonl-split` mode, entry and exit are
   separate events, so interleaved output is natural and needs no special
   markers. In `jsonl-merged` mode, each syscall is a single event.

5. **Type-tagged values.** Every value that stands on its own -- a syscall
   argument, a return value, a struct field, or a value-object element of a
   collection (`array`, `pair`, `fd_set`, `bitset`) -- carries a `"type"`
   field, so consumers can handle it generically. A type's own internal
   scalars are bare, their meaning fixed by the enclosing type: a `dev`'s
   `major`/`minor`, a `timespec_t`'s `tv_sec`, a `port_range`'s `lo`/`hi`, the
   CPU indices in a `cpu_set_t`.

6. **No comments.** Everything traditional strace puts in `/* ... */` is real
   data represented structurally -- except a comment that merely *reformats*
   data already present (an ISO-8601 rendering of an epoch `seconds` value,
   say), which the consumer derives itself. There is no comment concept in
   structured output.

7. **Semantic types everywhere.** Wherever strace internally knows a value's
   semantic type (fd, tid, pgid, etc.), it emits that type, not a bare `int`.

8. **Numeric `raw` values are strings.** All numeric `raw` values are JSON
   strings, to preserve exact representation and avoid IEEE 754 precision loss
   on large values.

## Document conventions

A few terms and shorthands recur throughout.

- **Value object.** A JSON object describing one traced value. Every value
  object has a `"type"` field naming its [value type](#value-type-reference),
  plus type-specific fields.

- **Type-tagged.** Values from the kernel are value objects (principle 5).
  Event-level fields (`event`, `pid`, `entering`, `delayed`, ...) and
  *meta-fields* are *not* type-tagged: they are plain JSON scalars.

- **Meta-field.** A field carrying strace's own bookkeeping about a value
  rather than kernel data -- for example `arg`, `inout`, `changed`, `split`,
  `injected`, `index`, `style`, `truncated`. Meta-fields are plain scalars
  (booleans, integers, strings) and their names have **no** leading
  underscore.

- **`raw` and `value`.** A scalar's primitive form is `raw` when it is a
  number (a string, per principle 8, that `sym` may annotate) and `value` when
  it is text or opaque bytes (`string`, `path`, `bytes`, `mac`, `uuid`,
  `char`). `sym` is a symbolic name for a `raw`. Optional fields appear only
  when the relevant data is available (e.g. `path` on an `fd` requires `-y`).

- **Traditional-output equivalences.** Many entries show the classic strace
  rendering of a value -- as a `Traditional:` line in prose, or inline as
  `(trad ...)`. These are illustrative aids for checking faithfulness, not part
  of the JSON.

- **Unknown types and fields.** The `type` vocabulary and field set may grow;
  the schema `version` bumps only on *breaking* changes. A consumer must ignore
  fields it does not recognize and treat an unrecognized `type` as opaque --
  reading `raw`/`value` if present and otherwise passing the object through
  untouched. New value types follow the same `type`-discriminator pattern so
  older consumers degrade gracefully.

## Syscall events

The syscall event is the core of the stream. This section covers its shape in
both format variants, its event-level fields, how in-out arguments and
mid-syscall changes are represented, split structs, return values, timing, and
injection.

### Event shape

Every syscall event has:

- `event`: `"syscall"`
- `pid`: the traced process (present on every event of any type)
- `syscall`: a [`syscall`](#syscall) value identifying the call
- `args`: a JSON array indexed by argument position
- `return`: a [return value](#the-return-field) (exit side only)

Each element of `args` is a value object with an extra `"arg"` meta-field
naming the parameter: `{"arg": "dirfd", "type": "const", ...}`. The name is
flattened into the value object rather than wrapping it
(`{arg, value: {...}}`) to reduce nesting.

When `-k`/`-kk` is in effect, a syscall event may also carry a `stack` field.
Which event carries it varies by mode and by syscall -- see
[Stack traces](#stack-traces).

#### `jsonl-split` mode (two events per syscall)

Entry and exit are separate events, distinguished by `entering`
(`true` for entry, `false` for exit). Entry carries in-args; exit carries
out-args. An argument absent from a given event appears as `null` at its
index; trailing nulls are omitted (a shorter array means the remaining args
are absent). An argument used for both input and output appears in both
events at the same index.

Entry:

```json
{
  "event": "syscall",
  "pid": 1234,
  "syscall": {"type": "syscall", "name": "openat", "scno": "257"},
  "entering": true,
  "args": [
    {"arg": "dirfd", "type": "const", "raw": "-100", "sym": "AT_FDCWD"},
    {"arg": "pathname", "type": "string", "value": "/dev/null"},
    {"arg": "flags", "type": "flags", "raw": "0", "groups": [{"raw": "0", "elems": []}]}
  ]
}
```

Exit:

```json
{
  "event": "syscall",
  "pid": 1234,
  "syscall": {"type": "syscall", "name": "openat", "scno": "257"},
  "entering": false,
  "args": [],
  "return": {"type": "fd", "raw": "3", "fd_info": {"type": "dev", "path": "/dev/null", "kind": "char", "major": "1", "minor": "3"}}
}
```

#### `jsonl-merged` mode (one event per syscall)

A single event per syscall, emitted at exit time. There is no `entering`
field. Each element of `args` is a two-element array `[entry_value,
exit_value]`:

- Entry-only argument: `[entry_value, null]`
- Exit-only argument: `[null, exit_value]`
- In-out argument: `[old_value, new_value]`
- Argument not printed: `[null, null]`

Trailing `[null, null]` pairs are omitted. Top-level in-out arguments are
represented naturally by the `[old, new]` pair.

```json
{
  "event": "syscall",
  "pid": 1234,
  "syscall": {"type": "syscall", "name": "openat", "scno": "257"},
  "args": [
    [{"arg": "dirfd", "type": "const", "raw": "-100", "sym": "AT_FDCWD"}, null],
    [{"arg": "pathname", "type": "string", "value": "/dev/null"}, null],
    [{"arg": "flags", "type": "flags", "raw": "0", "groups": [{"raw": "0", "elems": []}]}, null]
  ],
  "return": {"type": "fd", "raw": "3", "fd_info": {"type": "dev", "path": "/dev/null", "kind": "char", "major": "1", "minor": "3"}}
}
```

### Event-level fields

`pid` is present on every event; the rest are optional.

| Field | When present | Description |
|-------|--------------|-------------|
| `pid` | always | The traced process/thread |
| `entering_pid` | exit event, PID changed mid-syscall (non-leader `execve`) | The PID the syscall was entered under (the event's `pid` is the new one). Matches entry to exit in split mode; records the original PID in merged mode |
| `ip` | `-i` | Hex user-space address the syscall was invoked from (trad `[00007f1234abcdef]` before the name) |
| `timestamp` | `-t`/`-tt`/`-ttt` | A [`time`](#time-and-duration) value (absolute point in time) |
| `relative_timestamp` | `-r` | A [`duration`](#time-and-duration) value: elapsed time since the previous event |

### In-out arguments and value changes

The kernel can modify an argument during a syscall. Structured output
represents the before- and after-values in their natural places -- there is no
special `=>` syntax -- and marks the change with meta-fields. Two cases arise,
depending on whether the whole argument or only a field inside a struct
argument changed.

**Whole-argument change.** The argument carries a before-value and an
after-value:

- In `jsonl-split` mode they land on separate events at the same arg index:
  the before-value on the entry event, the after-value on the exit event. The
  entry argument carries `inout: true` (a forward hint that the kernel may
  write it back); the exit argument carries `changed: true` only when the
  value actually differs (its absence means unchanged, or not re-read).
- In `jsonl-merged` mode the argument's `[old_value, new_value]` pair carries
  both directly (see [Event shape](#event-shape)); the `inout`/`changed` hints,
  which exist to correlate the separate split-mode events, are not needed.

Split-mode entry, then exit:

```json
{"args": [{"arg": "offset", "type": "addr", "raw": "0x100", "inout": true}]}
{"args": [{"arg": "offset", "type": "addr", "raw": "0x200", "changed": true}]}
```

`inout`/`changed` can co-occur with `indirect` (a pointer-dereferenced value
that is also kernel-modified). Traditional output renders a whole-argument
change as `old => new` -- `0x100 => 0x200`, or a repeated struct
`{x=0, y=0} => {x=0, y=1}`.

**Field-level change inside a struct.** When only some fields of a struct
argument change, the struct is delivered as a [split struct](#split-structs):
each changed field carries `changed: true` on its exit half (the struct as a
whole does not). See [Split structs](#split-structs) for the mechanism and a
worked example. Traditional output renders only the changed fields inline:
`{x=0, y=0 => 1}`.

### Split structs

Some structs have their fields delivered partly on entry and partly on exit
(e.g. `recvmsg`'s `msghdr`, where `msg_namelen` is supplied on entry and the
rest on exit). Both halves carry `split: true`, so consumers can identify a
split struct from either side without relying on context. In `jsonl-split`
mode the halves arrive as separate events; in `jsonl-merged` mode they are the
two elements of the `[entry, exit]` pair. Either way, the logical struct value
is the union of fields from both halves.

**`changed` within a split struct.** When a split struct has a field in both
halves, `changed: true` on the exit field marks its value as having changed
from the entry value. Without `changed`, the two values are expected to be
identical and the exit value is the live one.

Example: `recvmsg` emits `msg_namelen` (the buffer size) on entry and the full
struct on exit. When the kernel returns a different length:

Entry:

```json
{"args": [..., {"arg": "msg", "type": "struct", "split": true, "fields": {
  "msg_namelen": {"type": "decimal", "raw": "110"}
}}]}
```

Exit:

```json
{"args": [..., {"arg": "msg", "type": "struct", "split": true, "fields": {
  "msg_namelen": {"type": "decimal", "raw": "36", "changed": true},
  "msg_name": ..., "msg_iov": ...
}}]}
```

Traditional output for the merged value is `msg_namelen=110 => 36`. When
`msg_namelen` is unchanged (no `changed` flag), it is just `msg_namelen=36`.

### Return, timing, and injection

The exit event's envelope is as shown under [Event shape](#event-shape); the
cases below vary only the `return` value and a few top-level fields. Full
return-value shapes are under [The return field](#the-return-field).

| Situation | Enabled by | Distinguishing fields | Traditional |
|-----------|-----------|-----------------------|-------------|
| Success | — | `"return"` is the typed result value | `= 0` |
| Error | — | `"return"` value with a sibling `errno` | `= -1 EFAULT (Bad address)` |
| Died mid-syscall | — | `"return": {"type":"unavailable"}` | `= ?` |
| Interrupted by signal | — | `"return": {"type":"unavailable","errno":{"type":"errno","sym":"ERESTART_RESTARTBLOCK",...}}` | `= ? ERESTART_RESTARTBLOCK (Interrupted by signal)` |
| Detached mid-syscall | — | synthetic exit; `"return": {"type":"detached"}` | `<detached ...>` |
| Timing | `--syscall-times` | top-level `time` (a `duration`) | `<1.000>` |
| Injected return | `-e inject=SET:retval=N` | `injected: true` on `return` | `(INJECTED)` |
| Injected args + syscall | `-e inject=SET:...:syscall=X` | `injected: true` on args; sibling `injected_syscall` field (the [`syscall`](#syscall) actually run) | `(INJECTED: args, retval)` |
| Delayed | `-e inject=SET:delay_enter=N` | top-level `delayed` (+ `delayed_by`, a `duration`) | `(DELAYED)` |

**Detached mid-syscall.** When strace detaches between a syscall's entry and
exit stops, the entry event is emitted normally (no `return`) and a *synthetic*
exit event marks that no real exit will follow: it finalizes the pending
syscall for streaming consumers and carries the `detached` marker for trad's
`<detached ...>` suffix. A separate
[detached event](#process-and-signal-events) follows for the process as a
whole.

```json
{
  "event": "syscall",
  "pid": 1234,
  "syscall": {"type": "syscall", "name": "ptrace", "scno": "101"},
  "entering": false,
  "args": [],
  "return": {"type": "detached"}
}
```

**Injection.** Each injected value carries `injected: true` individually. When
a `:syscall=X` clause also rewrites the call to a "pure" syscall, the event
gains a sibling `injected_syscall` field -- itself a [`syscall`](#syscall) value
naming the call the kernel actually runs (plain retval/error injection adds no
such field):

```json
{
  "event": "syscall",
  "pid": 1234,
  "syscall": {"type": "syscall", "name": "openat", "scno": "257"},
  "injected_syscall": {"type": "syscall", "name": "getpid", "scno": "39"},
  "entering": false,
  "args": [
    {"arg": "dirfd", "type": "const", "raw": "-100", "sym": "AT_FDCWD", "injected": true},
    {"arg": "pathname", "type": "string", "value": "/dev/null", "injected": true}
  ],
  "return": {"type": "fd", "raw": "7", "injected": true}
}
```

**Timing and delay** add fields at the top level of the exit event, beside
`return`. `time` (from `--syscall-times`) and `delayed` / `delayed_by` (from
delayed injection) are independent and may appear together; each is a
[`duration`](#time-and-duration) value:

```json
{
  "event": "syscall",
  "pid": 1234,
  "entering": false,
  "return": {"type": "unsigned", "raw": "0"},
  "time": {"type": "duration", "seconds": "1", "nanoseconds": "0", "precision": "6"},
  "delayed": true,
  "delayed_by": {"type": "duration", "seconds": "1", "nanoseconds": "0"}
}
```

## Other events

Besides syscalls, the stream carries a header, signal and process-state
events, and (with `-c`/`-C`) summary events.

### Header event

The first line of output. Carries the schema version, strace version, output
format, `capabilities`, and `options`.

`version` is incremented on breaking schema changes. `format` is
`"jsonl-split"` or `"jsonl-merged"` (see [Format variants](#format-variants)).
`capabilities` lists the optional build features compiled into this strace (the
same set `--version` reports). Each entry is an object with a `name`, an
optional `value` (for `name=value` features such as the stack unwinder), and
`disabled: true` for a feature that was checked for but not compiled in.

`options` records the rendering-related CLI options active when the trace was
captured:

| Field               | Type   | Description                                                          |
|---------------------|--------|----------------------------------------------------------------------|
| `xlat`              | string | `"abbrev"`, `"raw"`, or `"verbose"` (`-X`)                           |
| `follow_forks`      | bool   | true if `-f`/`--follow-forks`/`--always-show-pid`                    |
| `syscall_number`    | bool   | true if `-n`                                                         |
| `arg_names`         | bool   | true if `-N`                                                         |
| `timestamps`        | string | `"time"` (`-t`/`-tt`) or `"unix"` (`-ttt`); absent if no `-t`        |
| `strings_in_hex`    | string | `"none"`, `"non_ascii"` (`-x`), `"all"` (`-xx`), `"non_ascii_chars"` |
| `decode_pid_comm`   | bool   | true if `--decode-pids=comm`                                         |
| `acolumn`           | int    | alignment column for return values (`-a`, default 40)                |
| `output_separately` | bool   | true if `-ff`                                                        |

```json
{
  "event": "header",
  "version": 1,
  "strace_version": "6.13",
  "format": "jsonl-split",
  "options": {
    "xlat": "abbrev",
    "follow_forks": false,
    "syscall_number": false,
    "arg_names": false,
    "strings_in_hex": "none",
    "decode_pid_comm": false,
    "acolumn": 40,
    "output_separately": false
  },
  "capabilities": [
    {"name": "stack-trace", "value": "libdw"},
    {"name": "m32-mpers"},
    {"name": "mx32-mpers"}
  ]
}
```

### Process and signal events

Each carries `pid`. The `signal` field is a [`signal`](#simple-value-types)
value.

| `event` | Emitted when | Shape |
|---------|--------------|-------|
| `signal` | A signal is delivered (with siginfo) | `{"event":"signal","pid":1234,"signal":{...},"info":{"type":"struct","fields":{...}}}` |
| `stopped` | Stopped by a signal, no siginfo (trad `--- stopped by SIGTSTP ---`) | `{"event":"stopped","pid":1234,"signal":{...}}` |
| `killed` | Killed by a signal | `{"event":"killed","pid":1234,"signal":{...},"coredumped":true}` |
| `exit` | Process exited; `status` is an integer 0-255 | `{"event":"exit","pid":1234,"status":0}` |
| `detached` | strace detached from the process | `{"event":"detached","pid":1234}` |

- With `-k`/`-kk`, `signal` and `stopped` events may carry a `stack` field --
  see [Stack traces](#stack-traces).
- `detached`: if a syscall was in progress, it is completed first with
  `"return": {"type": "detached"}` (see
  [Detached mid-syscall](#return-timing-and-injection)).

### Summary events

Emitted by `-c` (count only) and `-C` (count + trace) modes. Three event
types make up a summary block.

**Summary header** -- emitted before summary rows when multiple personalities
are present, identifying the personality of the upcoming block.

- `personality` (integer) -- personality index (0 = native, 1 = second
  personality, etc.)
- `name` (string) -- personality name (e.g., `"32 bit"`, `"x32"`)

```json
{"event": "summary_header", "personality": 1, "name": "32 bit"}
```

Traditional: `System call usage summary for 32 bit mode:`

**Summary row** -- one per syscall. All stats fields are always present
regardless of `-U` column selection; the `columns` array tells consumers which
columns to display and in what order.

```json
{
  "event": "summary",
  "columns": ["calls", "syscall"],
  "syscall": "read",
  "calls": "5",
  "errors": "0",
  "time_percent": "45.23",
  "total_time": "0.001234",
  "avg_time": "0.000246",
  "min_time": "0.000100",
  "max_time": "0.000500"
}
```

Column name mapping (CSC enum -> JSON field):

- `CSC_TIME_100S` -> `time_percent` -- percentage of total time
- `CSC_TIME_TOTAL` -> `total_time` -- total seconds
- `CSC_TIME_MIN` -> `min_time` -- minimum seconds
- `CSC_TIME_MAX` -> `max_time` -- maximum seconds
- `CSC_TIME_AVG` -> `avg_time` -- average seconds
- `CSC_CALLS` -> `calls` -- call count
- `CSC_ERRORS` -> `errors` -- error count
- `CSC_SC_NAME` -> `syscall` -- syscall name

**Summary total** -- emitted once after all `summary` rows, carrying the
aggregate totals. Same field set as `summary` minus the `syscall` field (the
row represents all syscalls). The `columns` array is included so the totals
row renders with the same column selection.

```json
{
  "event": "summary_total",
  "columns": ["calls", "syscall"],
  "calls": "11",
  "errors": "2",
  "time_percent": "100.00",
  "total_time": "0.002728",
  "avg_time": "0.000248",
  "min_time": "0.000050",
  "max_time": "0.001000"
}
```

## Value type reference

Every value object has a `type` field. Optional fields appear only when the
data is available (e.g. `path` on an `fd` needs `-y`). Numeric `raw` values are
JSON strings (principle 8), and values with a symbolic translation carry both
`raw` and `sym` (principle 2).

Simple types are listed in the table below; types with real substructure or
variants have their own subsections. Bookkeeping fields that can appear on any
value are listed under [Meta-fields](#meta-fields).

### Simple value types

| `type` | Description | Example |
|--------|-------------|---------|
| `decimal` | Signed decimal integer | `{"type":"decimal","raw":"-1"}` |
| `unsigned` | Unsigned decimal integer | `{"type":"unsigned","raw":"42"}` |
| `hex` | Hexadecimal integer | `{"type":"hex","raw":"0x1234"}` |
| `octal` | Octal integer | `{"type":"octal","raw":"0755"}` |
| `addr` | Pointer; `raw` is `null` for NULL | `{"type":"addr","raw":"0x7fff1234"}` |
| `const` | Named constant. `sym` is the resolved name; when the value is unresolved, `sym` is absent and `table` names the xlat table (e.g. `"PROT"`); both are absent when there is no table at all | `{"type":"const","raw":"-100","sym":"AT_FDCWD"}` |
| `alternatives` | Several equally valid representations (trad joins with ` or `); `elems` are strings or value objects | `{"type":"alternatives","elems":[{"type":"addr","raw":"0xffffffff00000000"},{"type":"string","value":"/dev/null"}]}` (trad `0xffffffff00000000 or "/dev/null"`) |
| `signal` | Signal number and name | `{"type":"signal","raw":"9","sym":"SIGKILL"}` |
| `uid`, `gid` | User / group ID (`-1` shown as `-1`) | `{"type":"uid","raw":"1000"}` |
| `tid`, `tgid`, `pgid`, `sid` | Task / thread-group / process-group / session ID. Pid types may add `strace_pid` and `comm` | `{"type":"tid","raw":"4567"}` |
| `string` | Traced string from tracee memory | `{"type":"string","value":"hello"}` |
| `path` | Filesystem path (quoted like a string) | `{"type":"path","value":"/dev/null"}` |
| `sun_path` | Unix socket path; `abstract:true` for a leading-NUL address | `{"type":"sun_path","value":"/tmp/s.sock"}` |
| `bytes` | Binary data, always `\xHH`-escaped | `{"type":"bytes","value":"\\xde\\xad"}` |
| `char` | Single character (trad `'S'`) | `{"type":"char","value":"S"}` |
| `mac` | Hardware address | `{"type":"mac","value":"00:11:22:33:44:55"}` |
| `uuid` | 128-bit UUID | `{"type":"uuid","value":"550e8400-e29b-41d4-a716-446655440000"}` |
| `time_t` | Seconds since the epoch | `{"type":"time_t","raw":"1707840000"}` |
| `ticks` | Clock ticks; `hz` and `precision` let the consumer compute `raw/hz` seconds | `{"type":"ticks","raw":"60","hz":"100","precision":"2"}` |
| `kernel_version` | Packed version, decomposed | `{"type":"kernel_version","raw":"331520","major":"5","minor":"15","patch":"0"}` |
| `ip_addr` | IPv4/IPv6 address; `family` is a `const` | `{"type":"ip_addr","family":{"type":"const","raw":"2","sym":"AF_INET"},"addr":"127.0.0.1"}` |
| `ifindex` | Interface index; optional `ifname` | `{"type":"ifindex","raw":"1","ifname":"lo"}` |
| `port_range` | Packed port range; `lo`/`hi` present only when well-formed | `{"type":"port_range","raw":"0x10000400","lo":"1024","hi":"4096"}` |
| `dev` | Device number (standalone form); the `-yy` `fd_info` form is under [fd](#fd) | `{"type":"dev","raw":"0x103","major":"1","minor":"3"}` |
| `fract` | Fraction (trad `30/1`) | `{"type":"fract","numerator":"30","denominator":"1"}` |
| `ioprio` | I/O priority class and level | `{"type":"ioprio","raw":"16386","class":{"type":"const","raw":"2","sym":"IOPRIO_CLASS_BE"},"level":"2"}` |
| `pair` | Two-element `[a, b]` read from a pointer | `{"type":"pair","elems":[{"type":"fd","raw":"3"},{"type":"fd","raw":"4"}]}` |
| `cpu_set_t` | CPU affinity mask; `elems` are CPU numbers (strings, per principle 8) | `{"type":"cpu_set_t","elems":["0","1","2","3"]}` |
| `fd_set` | Set of fds (trad `[3</dev/null> ...]`) | `{"type":"fd_set","elems":[{"type":"fd","raw":"3","fd_info":{"type":"path","path":"/dev/null"}}]}` |
| `errno` | Error number on a failed syscall | `{"type":"errno","raw":"14","sym":"EFAULT","strerror":"Bad address"}` |
| `unavailable` | Value could not be obtained; optional nested `errno` for ERESTART | `{"type":"unavailable"}` |
| `detached` | Placeholder marking detach mid-syscall | `{"type":"detached"}` |

### Meta-fields

These bookkeeping fields (plain scalars, no leading underscore) can accompany a
value. They are defined once here and referenced, not repeated, in the type
descriptions.

| Field | Type | Meaning | On |
|-------|------|---------|-----|
| `arg` | string | Parameter name | top-level syscall args |
| `inout` | bool | Entry argument the kernel may write back (see [In-out arguments](#in-out-arguments-and-value-changes)) | entry args |
| `changed` | bool | Value differs from the entry value | exit args, split-struct fields |
| `indirect` | bool | Value was read through a pointer (trad `[value]`) | any value |
| `split` | bool | Struct delivered across entry and exit (see [Split structs](#split-structs)) | struct |
| `injected` | bool | Value is synthetic (`-e inject`) | args, `return` |
| `index` | value | Explicit array index (trad `[idx]=value`) | array elements |
| `truncated` | bool | Output abbreviated (e.g. `-s` limit) | string, struct, array |
| `rest_unreadable` | bool | Remaining struct bytes unreadable (trad `{..., ???}`) | struct |
| `extra_data` | string | Raw bytes beyond the known struct size | struct |
| `decode_error` | string | Why decoding stopped partway through | struct |
| `reserved_inline` | bool | Unexpectedly non-zero reserved range between named fields | struct field |
| `fetch_failed_addr` | string | Address where element reading stopped | array |
| `truncated_addr` | string | Address where list traversal stopped | query_list |
| `deleted` | bool | The resolved fd path has been deleted | `fd_info` path |
| `style` | string | Display hint (see [the `style` display hint](#the-style-display-hint)) | many |

### const, flags, and bitset

#### flags

Bitwise OR of constants. `raw` is the full combined value (hex string). The
`groups` array holds one or more groups, each a logical partition of that value
(e.g. an enum field vs. bitflags extracted from the same integer). A group is
`{raw, elems}`: `raw` is the value this group contributes (hex string), `elems`
its typed elements. In the common single-group case the group `raw` equals the
top-level `raw`.

Each element of `elems` is one of:

- **`const`** -- a matched symbolic constant.
- **`shift`** -- a shift-encoded value (trad `21<<MAP_HUGE_SHIFT`):
  `{"type":"shift","raw":"0x540000","value":"21","shift":"MAP_HUGE_SHIFT"}`.
- **`mode`** -- a [mode](#mode) fragment for arguments that combine flag bits
  with mode bits.
- **`quota_type`** -- a quota-type fragment (`USRQUOTA`, `GRPQUOTA`, ...):
  `{"type":"quota_type","raw":"0","sym":"USRQUOTA"}`.
- **`remainder`** -- unmatched bits: `{"type":"remainder","raw":"0x1","table":"O"}`.
  `table` names the xlat table the unmatched bits belong to, present only when
  the table is known and no bits from it matched.

**Traditional rendering**, per group: abbrev joins element syms with `|`;
verbose emits `group.raw /* elem1|elem2|... */`; raw emits just `group.raw`.
Groups are joined with `|`.

```json
{
  "type": "flags",
  "raw": "0x80200",
  "groups": [
    {
      "raw": "0x80200",
      "elems": [
        {"type": "const", "raw": "0x200", "sym": "O_TRUNC"},
        {"type": "const", "raw": "0x80000", "sym": "O_CLOEXEC"}
      ]
    }
  ]
}
```

Zero value is a single empty group: `{"type":"flags","raw":"0","groups":[{"raw":"0","elems":[]}]}`.
A partial decode keeps the remainder as a separate element:

```json
{
  "type": "flags",
  "raw": "0x80001",
  "groups": [
    {
      "raw": "0x80001",
      "elems": [
        {"type": "const", "raw": "0x80000", "sym": "O_CLOEXEC"},
        {"type": "remainder", "raw": "0x1"}
      ]
    }
  ]
}
```

**Compound flags (multiple groups).** Some arguments combine a multi-valued
field (extracted via a bitmask) with regular bitflags -- `mount` flags carry
`MS_MGC_VAL` OR'd with mount flags; `mmap` flags carry `MAP_TYPE` OR'd with
bitflags. Each logical partition is its own group:

```json
{
  "type": "flags",
  "raw": "0xc0ed000f",
  "groups": [
    {
      "raw": "0xc0ed0000",
      "elems": [{"type": "const", "raw": "0xc0ed0000", "sym": "MS_MGC_VAL"}]
    },
    {
      "raw": "0xf",
      "elems": [
        {"type": "const", "raw": "0x1", "sym": "MS_RDONLY"},
        {"type": "const", "raw": "0x2", "sym": "MS_NOSUID"}
      ]
    }
  ]
}
```

The `mmap` hugetlb page size appears as a `shift` element:
`{"raw":"0x540000","elems":[{"type":"shift","raw":"0x540000","value":"21","shift":"MAP_HUGE_SHIFT"}]}`.

**QCMD encoding.** The first argument of `quotactl`/`quotactl_fd` is a
two-group flags value -- one group for the command, one for the quota type
(a `quota_type` element). Traditional: `QCMD(cmd, quota_type)`.

```json
{
  "type": "flags",
  "raw": "0x800002",
  "groups": [
    {
      "raw": "0x800002",
      "elems": [{"type": "const", "raw": "0x800002", "sym": "Q_QUOTAOFF", "style": "abbrev"}]
    },
    {
      "raw": "0x0",
      "elems": [{"type": "quota_type", "raw": "0", "sym": "USRQUOTA", "style": "abbrev"}]
    }
  ]
}
```

A flags value may render as a bracketed set instead of pipe-joined flags via
`style: "bitset"` -- see [the `style` display hint](#the-style-display-hint).

#### bitset

A bitmask shown as set membership in square brackets. `raw` is the mask;
`elems` holds the decoded members, each a typed value object (so the element
type is self-evident). `inverted: true` when strace lists the *missing* members
instead (more than half the bits set); trad prepends `~`.

```json
{
  "type": "bitset",
  "raw": "0x3d",
  "elems": [
    {"type": "const", "raw": "0", "sym": "ICMP_ECHOREPLY"},
    {"type": "const", "raw": "3", "sym": "ICMP_DEST_UNREACH"}
  ]
}
```

Traditional: `[ICMP_ECHOREPLY ICMP_DEST_UNREACH]`; inverted, `~[...]`.

A **signal set** (`sigset`) is just a bitset whose `elems` are `signal` values:

```json
{
  "type": "bitset",
  "raw": "0x1fd",
  "inverted": true,
  "elems": [{"type": "signal", "raw": "9", "sym": "SIGKILL"}]
}
```

Traditional: `~[KILL]`.

### fd

File descriptor. Optional detail depends on `-y`/`-yy` and rides in `fd_info`,
a polymorphic object keyed by its own `type`.

```json
{"type": "fd", "raw": "3"}
{"type": "fd", "raw": "3", "fd_info": {"type": "path", "path": "/tmp/file"}}
```

| `fd_info.type` | Needs | Example (and extra fields) |
|----------------|-------|----------------------------|
| `path` | `-y` | `{"type":"path","path":"/tmp/file"}` -- optional `deleted:true` |
| `dev` | `-yy` | `{"type":"dev","path":"/dev/null","kind":"char","major":"1","minor":"3"}` -- optional `tty_index` |
| `socket` | `-yy` | `{"type":"socket","details":"TCP:[127.0.0.1:8080->127.0.0.1:80]"}` -- interim string, see [appendix](#appendix-non-normative-notes) |
| `pidfd` | `-yy` | `{"type":"pidfd","pid":"1234"}` or `{"type":"pidfd","path":"anon_inode:[pidfd]"}` |
| `signalfd` | `-yy` | `{"type":"signalfd","sigmask":{"type":"bitset","raw":"0x4002","elems":[{"type":"signal","raw":"2","sym":"SIGINT"},{"type":"signal","raw":"15","sym":"SIGTERM"}]}}` |
| `eventfd` | `-yy` | `{"type":"eventfd","count":"0","id":"1","semaphore":"0"}` |

The `dev` variant carries the device components `kind`/`major`/`minor` (no
packed `raw`, unlike the standalone [`dev`](#simple-value-types) argument
form).

**dirfd (AT_FDCWD).** The `*at()` family take a `dirfd` that is either a real
fd or the sentinel `AT_FDCWD`. There is no `dirfd` type: the arg is a
[`const`](#simple-value-types) for `AT_FDCWD` and an `fd` for a real
descriptor. Consumers switch on `type`:

```json
{"arg": "dirfd", "type": "const", "raw": "-100", "sym": "AT_FDCWD"}
{"arg": "dirfd", "type": "fd", "raw": "3", "fd_info": {"type": "path", "path": "/srv/strace"}}
```

### mode

File permission mode, in two forms.

Simple numeric mode (`chmod`, `umask`, ... where there is no file type):

```json
{"type": "mode", "raw": "0755"}
```

Symbolic mode with file-type decomposition (`stat`, `mknod`, ... where the mode
includes `S_IFMT` bits). `filetype` is a `const`, `special` an array of
special-bit names (`S_ISUID`, `S_ISGID`, `S_ISVTX`), `perm` the remaining
permission bits in octal. Traditional: `S_IFREG|0644` or `S_IFREG|S_ISUID|0755`.

```json
{
  "type": "mode",
  "raw": "0104755",
  "filetype": {"type": "const", "raw": "0100000", "sym": "S_IFREG"},
  "special": ["S_ISUID"],
  "perm": "0755"
}
```

For arguments encoded as `xlat_bits | mode_bits`, a `mode` value can appear as
an element inside a [`flags`](#flags) group.

### timespec_t and timeval_t

`struct timespec`/`struct timeval` with flat `tv_sec`+`tv_nsec` / `tv_sec`+
`tv_usec` fields (not nested in a `struct` wrapper). These carry the raw struct
fields only; when such a value is an absolute wall-clock instant, the consumer
formats it from `tv_sec`/`tv_nsec` itself (see
[time and duration](#time-and-duration)).

```json
{"type": "timespec_t", "tv_sec": "1739900000", "tv_nsec": "123456789"}
{"type": "timeval_t", "tv_sec": "1739900000", "tv_usec": "123456"}
```

For `utimensat`/`futimens`, `tv_nsec` may be the sentinel `UTIME_NOW`
(1073741823) or `UTIME_OMIT` (1073741822); `sym` then names it:

```json
{"type": "timespec_t", "tv_sec": "3735928559", "tv_nsec": "1073741823", "sym": "UTIME_NOW"}
```

Traditional (abbrev) `UTIME_NOW`; (verbose)
`{tv_sec=..., tv_nsec=1073741823} /* UTIME_NOW */`.

### time and duration

Two flat types express *semantic* time values, as opposed to the raw
`timespec_t`/`timeval_t` struct layouts:

- **`time`** -- an absolute point in time, as `seconds` + `nanoseconds` since
  the epoch. Used for the event-level [`timestamp`](#event-level-fields)
  (`-t`/`-tt`/`-ttt`).
- **`duration`** -- an elapsed interval, same `seconds` + `nanoseconds` shape.
  Used for the event-level [`relative_timestamp`](#event-level-fields) (`-r`)
  and for syscall [timing and delay](#return-timing-and-injection).

Both carry an optional `precision` -- the number of fractional-second digits the
user requested (e.g. `6` for microsecond `-tt`), a hint for traditional
rendering only. Neither carries a pre-formatted string: an ISO-8601 rendering is
fully derivable from `seconds`/`nanoseconds`, so consumers format it themselves,
in whatever timezone they choose (principle 6).

```json
{"type": "time", "seconds": "1739900000", "nanoseconds": "123456789", "precision": "9"}
{"type": "duration", "seconds": "1", "nanoseconds": "0", "precision": "6"}
```

### struct and array

#### struct

Named fields keyed by name. Field order matches trad output but is not
semantically significant. Struct-level and field-level bookkeeping
(`truncated`, `rest_unreadable`, `extra_data`, `decode_error`,
`reserved_inline`, `split`) is described under [Meta-fields](#meta-fields).

```json
{
  "type": "struct",
  "fields": {
    "flags": {"type": "flags", "raw": "0x3f", "groups": [{"raw": "0x3f", "elems": [{"type": "const", "raw": "0x1f", "sym": "FLAG_A"}, {"type": "const", "raw": "0x20", "sym": "FLAG_B"}]}]},
    "usage": {"type": "unsigned", "raw": "0"},
    "devid": {"type": "dev", "major": "0", "minor": "1"}
  }
}
```

#### array

Array of values in `elems`. `truncated` marks an abbreviated array;
`fetch_failed_addr` gives the address where reading stopped; elements may carry
an [`index`](#meta-fields) for explicitly-indexed arrays (trad `[idx]=value`).

```json
{
  "type": "array",
  "elems": [
    {
      "type": "unsigned",
      "raw": "3",
      "index": {"type": "const", "raw": "0", "sym": "VINTR"}
    },
    {
      "type": "unsigned",
      "raw": "28",
      "index": {"type": "const", "raw": "1", "sym": "VQUIT"}
    }
  ]
}
```

A **`query_list`** is a linked list flattened into the same array shape (e.g.
io_uring `IORING_REGISTER_PBUF_STATUS` entries); `truncated_addr` marks a
traversal cut short by limits.

### sockaddr

Socket address. Always carries `family` (a `const` naming the address family);
the remaining fields are family-specific. The common families are specified
below; rarer families follow trad output (see `src/sockaddr.c` for the
authoritative per-family list) and add their own fields under the same pattern.

| `family` sym | Fields |
|--------------|--------|
| `AF_INET` | `addr` (dotted string), `port` |
| `AF_INET6` | `addr` (string), `port`, `flowinfo`, `scope_id` |
| `AF_UNIX` | `path` (a [`sun_path`](#simple-value-types) value) |
| `AF_NETLINK` | `pid`, `groups` |

```json
{"type": "sockaddr", "family": {"type": "const", "raw": "2", "sym": "AF_INET"}, "addr": "127.0.0.1", "port": "8080"}
{"type": "sockaddr", "family": {"type": "const", "raw": "10", "sym": "AF_INET6"}, "addr": "::1", "port": "8080", "flowinfo": "0", "scope_id": "0"}
{"type": "sockaddr", "family": {"type": "const", "raw": "1", "sym": "AF_UNIX"}, "path": {"type": "sun_path", "value": "/tmp/s.sock"}}
{"type": "sockaddr", "family": {"type": "const", "raw": "16", "sym": "AF_NETLINK"}, "pid": "0", "groups": "0x1"}
```

### syscall

Syscall identifier. `name`/`scno` are the user-written syscall strace displays;
`scno` is always present.

```json
{"type": "syscall", "name": "openat", "scno": "257"}
```

When `-e inject=...:syscall=X` rewrites the call to a different "pure" syscall,
the event carries a separate top-level
[`injected_syscall`](#return-timing-and-injection) value -- another `syscall`
value -- for the call the kernel actually runs; this value is unchanged. Plain
retval/error injection (no `:syscall=` clause) adds no `injected_syscall` field
-- only the [return value](#the-return-field) carries `injected`.

### wait_status

Wait status from `wait4`/`waitpid`/`waitid`. `raw` is the packed status;
`status` selects the additional fields. Traditional output renders
`[{WIFEXITED(s) && WEXITSTATUS(s) == 42}]` and similar.

| `status` | Additional fields | Example |
|----------|-------------------|---------|
| `exited` | `exit_code` | `{"type":"wait_status","raw":"0x2a00","status":"exited","exit_code":"42"}` |
| `signaled` | `signal`, optional `coredumped` | `{"type":"wait_status","raw":"0x83","status":"signaled","signal":{"type":"signal","raw":"3","sym":"SIGQUIT"},"coredumped":true}` |
| `stopped` | `signal`, optional `ptrace_event` | `{"type":"wait_status","raw":"0x137f","status":"stopped","signal":{"type":"signal","raw":"19","sym":"SIGSTOP"}}` |
| `continued` | (none) | `{"type":"wait_status","raw":"0xffff","status":"continued"}` |

`ptrace_event` (a `const`) is present when ptrace event bits are set, e.g.
`{"type":"const","raw":"4","sym":"PTRACE_EVENT_EXEC"}`.

### ioctl_op

Ioctl command number, carrying symbolic name(s) plus the full `_IOC`
decomposition. Because different subsystems reuse command numbers, `syms` (when
present) lists the synonymous names as an array of strings.

| Field | Description |
|-------|-------------|
| `raw` | Full 32-bit ioctl number (hex) |
| `syms` | Optional; array of symbolic synonym strings |
| `dir` | Direction bits as a `flags` value (`_IOC_NONE`/`_IOC_READ`/`_IOC_WRITE`); always present |
| `ioc_type`, `ioc_nr`, `ioc_size` | The `_IOC` type byte, number, and size (hex) |

The decomposition is always present, even for known ioctls, so type byte and
direction are usable without parsing the name. `dir` names `_IOC_NONE`
explicitly rather than omitting it, since `_IOC_NONE` is not guaranteed 0 on
all architectures. Traditional: the symbolic name (or names joined with ` or `), or
`_IOC(dir, type, nr, size)` when `syms` is absent.

```json
{
  "type": "ioctl_op",
  "raw": "0xc0086409",
  "syms": ["DRM_IOCTL_GET_CAP"],
  "dir": {"type": "flags", "raw": "0x3", "groups": [{"raw": "0x3", "elems": [{"type": "const", "raw": "0x1", "sym": "_IOC_WRITE"}, {"type": "const", "raw": "0x2", "sym": "_IOC_READ"}]}]},
  "ioc_type": "0x64", "ioc_nr": "0x09", "ioc_size": "0x8"
}
```

### uring_restriction_op

`struct io_uring_restriction`'s opcode byte and the union byte after it form a
discriminated union: the second byte's meaning depends on the first. This type
aggregates both so consumers need no syscall-specific knowledge.

- `raw` -- opcode value (hex); `sym` -- opcode name, present when resolved;
  when the opcode is unknown, `sym` is absent and `table` is
  `"IORING_RESTRICTION"`.
- Exactly one union field, chosen by `raw`: `register_op` (const), `sqe_op`
  (const), `sqe_flags` (flags), or -- when the opcode is unknown -- `op`, the
  raw union byte as a hex string.

```json
{"type": "uring_restriction_op", "raw": "0", "sym": "IORING_RESTRICTION_REGISTER_OP", "register_op": {"type": "const", "raw": "0", "sym": "IORING_REGISTER_BUFFERS"}}
{"type": "uring_restriction_op", "raw": "0x4", "table": "IORING_RESTRICTION", "op": "0xef"}
```

Traditional output renders the union field as a sibling of `opcode`
(`opcode=..., register_op=...`), or as a second comment for the unknown case
(`opcode=0x4 /* IORING_RESTRICTION_??? */ /* op: 0xef */`).

### The return field

The syscall event's `return` field holds the return value as a typed value
object *directly* -- `type`, `raw`, `sym`, ... sit at the top level, with no
`{"type":"return","value":...}` wrapper. Extra annotations are siblings in the
same object.

```json
"return": {"type": "unsigned", "raw": "0"}
"return": {"type": "fd", "raw": "3", "fd_info": {"type": "path", "path": "/dev/null"}}
"return": {"type": "decimal", "raw": "-1", "errno": {"type": "errno", "raw": "14", "sym": "EFAULT", "strerror": "Bad address"}}
"return": {"type": "fd", "raw": "7", "injected": true}
"return": {"type": "const", "raw": "5", "sym": "TIME_INS"}
"return": {"type": "unavailable"}
"return": {"type": "detached"}
```

In order above: success; fd return; error (the [`errno`](#simple-value-types)
rides as a sibling); injected value; a symbolic annotation on the value itself
(trad `= 5 (TIME_INS)`, e.g. `adjtimex`); no value available; and detach
mid-syscall. Some not-yet-converted decoders still emit a legacy `retstr`
sibling instead of a typed value -- see the [appendix](#appendix-non-normative-notes).

### Stack traces

With `-k` (frame symbols) or `-kk` (frames + source lines), an event carries an
optional `stack` field when the unwinder produced at least one frame or error.
`stack` appears on:

- **Syscall exit events** (the common case). The stack is captured at syscall
  *entry* -- so it reflects the invocation point, unaffected by mapping changes
  the syscall makes -- and attached to the exit event (the `"entering": false`
  event in split mode; the single event in merged mode).
- **Syscall entering events** in split mode, only for syscalls with no later
  event to carry it: `execve`/`execveat` (address space replaced by exit) and
  `exit`/`exit_group` (no exit event). This keeps frames visible even when the
  syscall never reaches exit. In merged mode there is no entering event, so
  such frames ride the merged event -- and if the syscall never reaches exit,
  no event is emitted for it at all.
- **Signal events** and **stopped events**.

No other events carry stack data.

```json
"stack": {
  "frames": [
    {"binary": "/lib/x86_64-linux-gnu/libc.so.6", "symbol": "openat", "offset": "0x42", "address": "0x7f9d3e3a8d33"},
    {"binary": "./a.out", "address": "0x4006bb"}
  ],
  "errors": [{"message": "too many stack frames"}]
}
```

`frames` is always present (possibly empty). Per frame:

| Field | Type | When present |
|-------|------|--------------|
| `binary` | string | always; path to the ELF object (empty string if unresolved) |
| `address` | string | always; hex IP |
| `symbol` | string | when a function symbol resolved (demangled with libiberty) |
| `offset` | string | only with `symbol`; hex offset within it |
| `source_file` | string | only with `-kk` and DWARF-resolved location |
| `source_line` | integer | only with `source_file` |

`address` and `offset` are bare hex strings (not typed `addr`/`hex` objects) to
keep dense traces compact -- a deliberate exception to type-tagging.

`errors` is omitted when empty. Each error has `message` (the unwinder's text)
and, when the failure-point IP is known, `address`. In trad rendering all
frames render first, then all errors -- the unwinder emits errors only after it
stops walking.

## The `style` display hint

`style` is an optional meta-field on value objects. It does not change the
meaning of the data, only how *traditional* strace formats it for
human-readable output. The underlying semantic value is fully described by the
other fields, so a consumer that only wants the data may ignore `style`.

It exists because a few decoders deliberately override the global `-X` setting
for specific values (e.g. `syslog` forces verbose xlat style). Rather than
making a reader special-case those syscalls, the override is carried
intrinsically on the affected value.

**On string/path values** -- controls byte escaping for non-printable
characters:

| Value         | Meaning                                       | C origin              |
|---------------|-----------------------------------------------|-----------------------|
| (absent)      | Default: octal `\NNN`, named `\n` etc.        | Normal string quoting |
| `"non_ascii"` | Hex for bytes > 0x7f, named for control chars | `-x` mode per-value   |

Binary data that needs `\xHH` escaping uses [`bytes`](#simple-value-types) instead of a
`style` hint.

**On const/flags values** -- selects xlat verbosity regardless of the global
`-X` setting:

- `"verbose"` -- format as `raw /* SYMBOLIC */`. Some syscalls (e.g. `syslog`)
  hardcode this.
- `"abbrev"` -- always use the abbreviated `SYMBOLIC` form.

Without `style`, the global `-X` setting selects between raw, abbrev, and
verbose.

```json
{"type": "const", "style": "verbose", "raw": "2", "sym": "SYSLOG_ACTION_READ"}
```

**On flags values (`"bitset"`)** -- render as a space-separated set in square
brackets (like the [`bitset`](#bitset) type) rather than pipe-separated flags.
Used by ICMP_FILTER and similar socket options. Traditional:
`[ELEM1 ELEM2]`.

```json
{"type": "flags", "style": "bitset", "raw": "0x3d", "groups": [{"raw": "0x3d", "elems": [...]}]}
```

**On mode values (`"octal"`)** -- format the raw value with a leading zero
(`0755`, not `493` or `0x1ed`). This is the standard for mode values.

```json
{"type": "mode", "style": "octal", "raw": "0755"}
```

## Options that affect output

Structured output implies no extra data-fetching (principle 1). These options
control what data strace collects; structured output simply formats whatever
was collected.

| Option           | Effect                            | Structured output                                                                       |
|------------------|-----------------------------------|-----------------------------------------------------------------------------------------|
| `-y`             | Resolve fd paths                  | `fd_info` with `type: "path"` on `fd`                                                    |
| `-yy`            | Detailed fd info (device, socket) | `fd_info` with `type: "dev"`, etc.                                                       |
| `-k` / `-kk`     | Stack traces                      | `stack` field with `frames[]` + optional `errors[]` (see [Stack traces](#stack-traces)) |
| `-s N`           | String truncation limit           | `truncated` field on `string` values                                                    |
| `-e verbose=set` | Decode structures in detail       | More fields in `struct` values                                                          |

The `-X raw`/`-X verbose`/`-X abbrev` options affect only traditional text
formatting. Structured output always emits both `raw` and `sym` when available
(principle 2). Per-value overrides of the global `-X` setting are communicated
via [`style`](#the-style-display-hint).

## Worked example: sysinfo

Traditional output:

```
sysinfo({uptime=6252125, loads=[643424, 318720, 119712], totalram=1081646206976,
  freeram=736271814656, sharedram=61651206144, bufferram=1085440,
  totalswap=1920383406080, freeswap=1920204382208, procs=2612, totalhigh=0,
  freehigh=0, mem_unit=1}) = 0
```

Structured JSONL. The entry event has no args since all fields are
out-parameters:

```json
{
  "event": "syscall",
  "pid": 1234,
  "syscall": {"type": "syscall", "name": "sysinfo", "scno": "99"},
  "entering": true,
  "args": []
}
```

```json
{
  "event": "syscall",
  "pid": 1234,
  "syscall": {"type": "syscall", "name": "sysinfo", "scno": "99"},
  "entering": false,
  "args": [
    {
      "arg": "info",
      "type": "struct",
      "fields": {
        "uptime": {"type": "unsigned", "raw": "6252125"},
        "loads": {
          "type": "array",
          "elems": [
            {"type": "unsigned", "raw": "643424"},
            {"type": "unsigned", "raw": "318720"},
            {"type": "unsigned", "raw": "119712"}
          ]
        },
        "totalram": {"type": "unsigned", "raw": "1081646206976"},
        "freeram": {"type": "unsigned", "raw": "736271814656"},
        "sharedram": {"type": "unsigned", "raw": "61651206144"},
        "bufferram": {"type": "unsigned", "raw": "1085440"},
        "totalswap": {"type": "unsigned", "raw": "1920383406080"},
        "freeswap": {"type": "unsigned", "raw": "1920204382208"},
        "procs": {"type": "unsigned", "raw": "2612"},
        "totalhigh": {"type": "unsigned", "raw": "0"},
        "freehigh": {"type": "unsigned", "raw": "0"},
        "mem_unit": {"type": "unsigned", "raw": "1"}
      }
    }
  ],
  "return": {"type": "unsigned", "raw": "0"}
}
```

## Appendix: non-normative notes

This section groups notes that are intentionally *not* part of the stable
schema: interim shapes, planned work, and legacy fields. They are collected
here so the reference above stays free of "will change" caveats.

- **Interim `fd_info.type: "socket"`.** The socket `fd_info` currently carries
  a legacy string from socketutils:
  `{"type": "fd", "raw": "3", "fd_info": {"type": "socket", "details": "TCP:[127.0.0.1:8080->127.0.0.1:80]"}}`.
- **Planned structured socket variants.** A future phase will replace the
  interim form with `socket_inet`, `socket_unix`, `socket_netlink`:

  ```json
  {"type": "fd", "raw": "3", "fd_info": {"type": "socket_inet", "protocol": "TCP", "family": "AF_INET", "src_addr": "192.168.1.1", "src_port": "8080", "dst_addr": "192.168.1.2", "dst_port": "80"}}
  {"type": "fd", "raw": "3", "fd_info": {"type": "socket_unix", "protocol": "UNIX", "inode": "12345", "peer_inode": "67890", "path": "/tmp/socket.sock"}}
  {"type": "fd", "raw": "3", "fd_info": {"type": "socket_netlink", "protocol": "NETLINK", "nl_protocol": "ROUTE", "portid": "0"}}
  ```

- **Legacy `retstr`.** Some decoders still emit a `retstr` field as a sibling
  on the return object instead of a properly typed return value. This is a
  transitional shape to be removed as those decoders are converted.
- **Future value types.** Additional value types may be added as needed,
  following the same pattern -- a `"type"` discriminator plus type-specific
  fields.
