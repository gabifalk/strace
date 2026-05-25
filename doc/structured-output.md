# strace Structured JSONL Output

*Draft -- describes proposed schema version 1.*

## Overview

strace can emit its trace as a JSONL (JSON Lines) stream: one JSON object
per line, one line per event. Two format variants are available:

- **`-B jsonl-split`** (alias: `-B jsonl`) -- two events per syscall
  (entering + exiting), the default JSONL mode.
- **`-B jsonl-merged`** -- one event per syscall, with each argument emitted
  as a `[entry_value, exit_value]` pair. Entry arguments are buffered at
  syscall entry and combined with exit arguments at exit time.

**Limitation of `jsonl-merged`:** each syscall produces a single event
emitted only at exit time, so information that depends on the *moment of
entry* is lost. In particular:

- **No real-time entry visibility.** A long-running syscall (a blocking
  `read`, a `sleep`, etc.) produces no output at all until it returns;
  traditional and `jsonl-split` both surface the entry stop immediately.
- **No interleaving across pids/threads.** Concurrent syscalls in
  different threads appear sequentially in exit order rather than in the
  order they entered vs. returned. Traditional output's
  `<unfinished ...>` / `<... resumed>` mechanism and `jsonl-split`'s
  separate entering/exiting events both preserve this ordering;
  `jsonl-merged` does not.

**About the JSON examples in this document:** in actual JSONL output
every event occupies exactly one line -- no embedded newlines, no
indentation. The JSON snippets here are pretty-printed (multi-line,
indented) purely for readability; each one corresponds to a single
physical line of strace's output.

## Contents

- [Overview](#overview)
- [Invocation](#invocation)
- [Core Principles](#core-principles)
- [Event Types](#event-types)
  - [Header Event](#header-event)
  - [Syscall Event](#syscall-event)
  - [Signal Event](#signal-event)
  - [Exit Event](#exit-event)
  - [Killed Event](#killed-event)
  - [Stopped Event](#stopped-event)
  - [Detached Event](#detached-event)
  - [Summary Header Event](#summary-header-event)
  - [Summary Event](#summary-event)
  - [Summary Total Event](#summary-total-event)
- [Value Types](#value-types)
  - [decimal](#decimal)
  - [unsigned](#unsigned)
  - [hex](#hex)
  - [octal](#octal)
  - [addr](#addr)
  - [const](#const)
  - [flags](#flags)
  - [bitset](#bitset)
  - [sigset](#sigset)
  - [alternatives](#alternatives)
  - [syscall](#syscall)
  - [signal](#signal)
  - [fd](#fd)
  - [tid, tgid, pgid, sid](#tid-tgid-pgid-sid)
  - [uid, gid](#uid-gid)
  - [string](#string)
  - [bytes](#bytes)
  - [path](#path)
  - [sun_path](#sun_path)
  - [mac](#mac)
  - [uuid](#uuid)
  - [mode](#mode)
  - [time_t](#time_t)
  - [ticks](#ticks)
  - [timespec_t](#timespec_t)
  - [timeval_t](#timeval_t)
  - [sockaddr](#sockaddr)
  - [ip_addr](#ip_addr)
  - [ifindex](#ifindex)
  - [ioctl_op](#ioctl_op)
  - [uring_restriction_op](#uring_restriction_op)
  - [struct](#struct)
  - [array](#array)
  - [query_list](#query_list)
  - [cpu_set_t](#cpu_set_t)
  - [fd_set](#fd_set)
  - [pair](#pair)
  - [dev](#dev)
  - [char](#char)
  - [fract](#fract)
  - [port_range](#port_range)
  - [ioprio](#ioprio)
  - [wait_status](#wait_status)
  - [kernel_version](#kernel_version)
  - [return field](#return-field)
  - [errno](#errno)
  - [unavailable](#unavailable)
- [Syscall Event Details](#syscall-event-details)
  - [Event-level fields](#event-level-fields)
  - [In-out arguments and value changes](#in-out-arguments-and-value-changes)
  - [`split` structs](#split-structs)
  - [`changed` within `split` structs](#changed-within-split-structs)
  - [Error return](#error-return)
  - [No return (process died mid-syscall)](#no-return-process-died-mid-syscall)
  - [Detached mid-syscall](#detached-mid-syscall)
  - [Interrupted by signal](#interrupted-by-signal)
  - [Syscall timing (`--syscall-times`)](#syscall-timing---syscall-times)
  - [Syscall injection (`-e inject`)](#syscall-injection--e-inject)
  - [Delayed injection (`-e inject=SET:delay_enter=N`)](#delayed-injection--e-injectsetdelay_entern)
- [Complete Example: sysinfo](#complete-example-sysinfo)
- [Special Cases](#special-cases)
  - [dirfd (AT_FDCWD)](#dirfd-at_fdcwd)
  - [UTIME_NOW / UTIME_OMIT](#utime_now-utime_omit)
- [Data-Fetching Options and Their Effect on Structured Output](#data-fetching-options-and-their-effect-on-structured-output)
- [The `style` Display Hint](#the-style-display-hint)
  - [`style` on string/path values](#style-on-stringpath-values)
  - [`style` on struct values (timestamp)](#style-on-struct-values-timestamp)
  - [`style` on mode values](#style-on-mode-values)
  - [`style` on const/flags values (xlat verbosity)](#style-on-constflags-values-xlat-verbosity)
  - [`style: "bitset"` on flags values](#style-bitset-on-flags-values)
- [Presentation Options (`-X`) and Structured Output](#presentation-options-x-and-structured-output)
- [Interim and Future Work](#interim-and-future-work)

## Invocation

```
strace -B jsonl-split   <command>     # two events per syscall
strace -B jsonl         <command>     # alias for the above
strace -B jsonl-merged  <command>     # one event per syscall
```

All other strace flags work as usual. The flags that control what data
strace collects (`-y`, `-yy`, `-k`, `-s`, `-e verbose=...`, etc.) determine
which optional fields the structured output carries -- see
[Data-Fetching Options](#data-fetching-options-and-their-effect-on-structured-output).

## Core Principles

1. **Presentation only** -- structured output is a different format for the same
   data. It never implies extra data-fetching beyond what the user requested
   with flags like `-y`, `-yy`, `-k`, etc.

2. **Always emit raw + symbolic** -- for values that have symbolic translations
   (constants, flags), always include both `raw` and `sym` fields, regardless
   of `-X` setting. This is free since both values are already available
   internally.

3. **Structured decomposition** -- whenever strace has structured data
   internally, emit structured fields, not formatted strings. For example,
   device info becomes `{"type": "dev", "kind": "char", "major": "1", "minor": "3"}`,
   not `"char 1:3"`.

4. **No unfinished/resumed** -- in `jsonl-split` mode, entry and exit are
   separate events; interleaved output is natural and needs no special markers.
   In `jsonl-merged` mode, each syscall is a single event.

5. **Type-tagged values** -- every traced value from the kernel carries a
   `"type"` field so consumers can handle it generically. Event-level
   fields (`event`, `pid`, `entering`, `delayed`, ...) and value-level
   meta-fields (strace's own bookkeeping) are plain JSON scalars
   (booleans, integers, strings) and are *not* type-tagged.

6. **No comments** -- everything that traditional strace puts in `/* ... */` is
   actual data that must be represented structurally. There is no comment
   concept in structured output.

7. **Semantic types everywhere** -- wherever strace internally knows a value's
   semantic type (fd, tid, pgid, etc.), emit that type, not just `int`.

8. **Numeric values as strings** -- all numeric `raw` values are JSON strings
   to preserve exact representation and avoid IEEE 754 precision loss on large
   values.

## Event Types

### Header Event

Emitted as the first line of output. Contains the schema version, strace
version, output format, `capabilities`, and `options`.

`version` is incremented on breaking schema changes.

`format` identifies the output format variant:
- `"jsonl-split"` -- two events per syscall (entering + exiting)
- `"jsonl-merged"` -- one event per syscall with `[entry, exit]` arg pairs

Consumers can use `format` to determine how to parse syscall events
(whether to expect `entering` fields and separate entry/exit events, or
merged `[entry, exit]` argument arrays).

`capabilities` is a list of optional build features.

`options` contains the rendering-related CLI options that were active
when the trace was captured:

| Field               | Type   | Description                                                          |
|---------------------|--------|----------------------------------------------------------------------|
| `xlat`              | string | `"abbrev"`, `"raw"`, or `"verbose"` (`-X`)                           |
| `follow_forks`      | bool   | true if `-f`/`--follow-forks`/`--always-show-pid`                    |
| `syscall_number`    | bool   | true if `-n`                                                         |
| `arg_names`         | bool   | true if `-N`                                                         |
| `timestamps`        | string | `"time"` (`-t`/`-tt`) or `"unix"` (`-ttt`); absent if no `-t`        |
| `strings_in_hex`    | string | `"none"`, `"non-ascii"` (`-x`), `"all"` (`-xx`), `"non-ascii-chars"` |
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
  "capabilities": ["stack-trace=libdw", "m32-mpers", "mx32-mpers"]
}
```

### Syscall Event

#### `jsonl-split` mode (two events per syscall)

Entry and exit are separate events. `entering: true` for entry,
`entering: false` for exit. `args` is a JSON array indexed by argument
position. Entry has in-args, exit has out-args. Args not present in a
given event appear as `null` at their index. Trailing nulls are omitted
(a shorter array means the remaining args are absent). An argument used
for both input and output appears in both events at the same index.

Each argument is a type-tagged value object with an additional `"arg"` field:
`{"arg": "dirfd", "type": "const", ...}`. The arg name is flattened into the
value object rather than wrapping it (`{arg, value: {type, ...}}`) to reduce
nesting.

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
    {"arg": "flags", "type": "flags", "raw": "0", "flags": []}
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
field. `args` is a JSON array where each element is a two-element array
`[entry_value, exit_value]`:

- Entry-only argument: `[entry_value, null]`
- Exit-only argument: `[null, exit_value]`
- In-out argument: `[old_value, new_value]`
- Argument not printed: `[null, null]`

Trailing `[null, null]` pairs are omitted. Top-level in-out arguments are
naturally represented by the `[old, new]` pair.

```json
{
  "event": "syscall",
  "pid": 1234,
  "syscall": {"type": "syscall", "name": "openat", "scno": "257"},
  "args": [
    [{"arg": "dirfd", "type": "const", "raw": "-100", "sym": "AT_FDCWD"}, null],
    [{"arg": "pathname", "type": "string", "value": "/dev/null"}, null],
    [{"arg": "flags", "type": "flags", "raw": "0", "flags": []}, null]
  ],
  "return": {"type": "fd", "raw": "3", "fd_info": {"type": "dev", "path": "/dev/null", "kind": "char", "major": "1", "minor": "3"}}
}
```

### Signal Event

```json
{
  "event": "signal",
  "pid": 1234,
  "signal": {"type": "signal", "raw": "17", "sym": "SIGCHLD"},
  "info": {"type": "struct", "fields": {"si_signo": {...}, "si_code": {...}, ...}}
}
```

### Exit Event

```json
{"event": "exit", "pid": 1234, "status": 0}
```

`status` is always an integer (0-255).

### Killed Event

```json
{
  "event": "killed",
  "pid": 1234,
  "signal": {"type": "signal", "raw": "11", "sym": "SIGSEGV"},
  "coredumped": true
}
```

### Stopped Event

Emitted when a process is stopped by a signal (without siginfo). When siginfo
is available, a `signal` event is emitted instead. Traditional output:
`--- stopped by SIGTSTP ---`.

```json
{
  "event": "stopped",
  "pid": 1234,
  "signal": {"type": "signal", "raw": "20", "sym": "SIGTSTP"}
}
```

### Detached Event

Emitted when strace detaches from a process. If a syscall was in progress,
the syscall event is completed first with `"return": {"type": "detached"}`
(see [Detached mid-syscall](#detached-mid-syscall)).

```json
{"event": "detached", "pid": 1234}
```

### Summary Header Event

Emitted before summary rows when multiple personalities are present. Contains
a human-readable message identifying the personality.

```json
{"event": "summary_header", "message": "System call usage summary for 32 bit mode:"}
```

### Summary Event

Emitted by `-c` (count only) and `-C` (count + trace) modes, one event per
syscall row. All stats fields are always present regardless of `-U` column
selection. The `columns` array tells consumers which columns to display
and in what order.  The aggregate totals are emitted as a separate
[Summary Total Event](#summary-total-event) (always last).

```json
{"event": "summary", "columns": ["calls", "syscall"], "syscall": "read", "calls": "5", "errors": "0", "time_percent": "45.23", "total_time": "0.001234", "avg_time": "246", "min_time": "0.000100", "max_time": "0.000500"}
```

Column name mapping (CSC enum -> JSON field):
- `CSC_TIME_100S` -> `"time_percent"` -- percentage of total time
- `CSC_TIME_TOTAL` -> `"total_time"` -- total seconds
- `CSC_TIME_MIN` -> `"min_time"` -- minimum seconds
- `CSC_TIME_MAX` -> `"max_time"` -- maximum seconds
- `CSC_TIME_AVG` -> `"avg_time"` -- average microseconds (integer)
- `CSC_CALLS` -> `"calls"` -- call count
- `CSC_ERRORS` -> `"errors"` -- error count
- `CSC_SC_NAME` -> `"syscall"` -- syscall name

### Summary Total Event

Emitted once after all `summary` events, carrying the aggregate totals
across all syscalls.  Same field set as `summary` minus the `syscall`
field (the row represents all syscalls, not one of them).  The
`columns` array is included so the totals row can be rendered with the
same column selection as the per-syscall rows.

```json
{"event": "summary_total", "columns": ["calls", "syscall"], "calls": "11", "errors": "2", "time_percent": "100.00", "total_time": "0.002728", "avg_time": "248", "min_time": "0.000050", "max_time": "0.001000"}
```

## Value Types

Every value object has a `"type"` field. Optional fields only appear when the
relevant data is available (e.g., `path` on `fd` requires `-y`). All numeric
`raw` values are JSON strings.

### decimal

Signed decimal integer.

```json
{"type": "decimal", "raw": "-1"}
```

### unsigned

Unsigned decimal integer.

```json
{"type": "unsigned", "raw": "42"}
```

### hex

Hexadecimal integer.

```json
{"type": "hex", "raw": "0x1234"}
```

### octal

Octal integer.

```json
{"type": "octal", "raw": "0755"}
```

### addr

Pointer / address. `null` for NULL pointers.

```json
{"type": "addr", "raw": "0x7fff1234"}
{"type": "addr", "raw": null}
```

### const

Single named constant. The `sym` field carries the symbolic name.

```json
{"type": "const", "raw": "-100", "sym": "AT_FDCWD"}
```

When strace knows the table but not the value:

```json
{"type": "const", "raw": "1", "sym": "BTRFS_FEATURE_COMPAT_???"}
```

When strace has no symbolic name at all, `sym` is absent:

```json
{"type": "const", "raw": "32"}
```

**Note:** Structured output always includes `raw` when the value is known,
regardless of the `-X` setting. Some syscall decoders (e.g. syslog)
unconditionally use verbose xlat style in their traditional output regardless
of the `-X` setting.

### flags

Bitwise OR of constants. The `flags` array contains one or more **groups**.
Each group represents a logical partition of the combined value (e.g., an
enum field vs. bitflags extracted from the same integer). A group is an
object with:

- **`raw`** -- the numeric value contributed by this group (hex string)
- **`elems`** -- array of typed element objects belonging to this group

Each element in `elems` is one of these kinds:

- **`const`** -- a matched symbolic constant:
  `{"type": "const", "raw": "0x80000", "sym": "O_CLOEXEC"}`
- **shift expression** -- a plain string for shift-encoded values:
  `"21<<MAP_HUGE_SHIFT"`
- **`mode`** -- a mode fragment for composite arguments that combine
  xlat/flag bits with mode bits.
- **`quota_type`** -- a quota type fragment for composite arguments (e.g.,
  `USRQUOTA`, `GRPQUOTA`, `PRJQUOTA`), encoded as a typed value:
  `{"type": "quota_type", "raw": "0", "sym": "USRQUOTA"}`.
- **`remainder`** -- unmatched bits:
  `{"type": "remainder", "raw": "0x1", "sym": "O_???"}`.
  The `sym` field carries the xlat table hint (e.g., `"CLOSE_RANGE_???"`)
  and is only present when the table is known and no bits from it matched.

When all bits matched and there is no remainder, no `remainder` element
appears. When some bits are unmatched but the xlat table hint has already
been used by a matched constant from the same table, the `remainder` element
omits `sym`.

#### Traditional output

For reference, the traditional renderings strace uses per group are:

- **Abbrev** (`-Xabbrev`): per group, element syms joined with `|`.
- **Verbose** (`-Xverbose`): per group, `group.raw /* elem1|elem2|... */`.
- **Raw** (`-Xraw`): per group, just `group.raw`.

Groups themselves are joined with `|`.

#### Examples

Fully decoded (single flag, one group):

```json
{"type": "flags", "flags": [{"raw": "0x80000", "elems": [{"type": "const", "raw": "0x80000", "sym": "O_CLOEXEC"}]}]}
```

Multiple flags (one group):

```json
{"type": "flags", "flags": [{"raw": "0x80200", "elems": [{"type": "const", "raw": "0x200", "sym": "O_TRUNC"}, {"type": "const", "raw": "0x80000", "sym": "O_CLOEXEC"}]}]}
```

Zero value:

```json
{"type": "flags", "flags": [{"raw": "0", "elems": []}]}
```

Unknown value (no symbolic match):

```json
{"type": "flags", "flags": [{"raw": "0x1", "elems": [{"type": "remainder", "raw": "0x1", "sym": "CLOSE_RANGE_???"}]}]}
```

Partially decoded (some bits matched, remainder separate):

```json
{"type": "flags", "flags": [{"raw": "0x80001", "elems": [{"type": "const", "raw": "0x80000", "sym": "O_CLOEXEC"}, {"type": "remainder", "raw": "0x1"}]}]}
```

#### Compound flags (multiple groups)

Some syscall arguments combine a multi-valued field (extracted via a bitmask)
with regular bitflags. For example, `mount` flags contain `MS_MGC_VAL` (a
magic number) OR'd with regular mount flags. Similarly, `mmap` flags contain
`MAP_TYPE` (a 4-bit enum) OR'd with bitflags like `MAP_ANONYMOUS`.

These are emitted with one group per logical partition:

```json
{"type": "flags", "flags": [{"raw": "0xc0ed0000", "elems": [{"type": "const", "raw": "0xc0ed0000", "sym": "MS_MGC_VAL"}]}, {"raw": "0xf", "elems": [{"type": "const", "raw": "0x1", "sym": "MS_RDONLY"}, {"type": "const", "raw": "0x2", "sym": "MS_NOSUID"}, {"type": "const", "raw": "0x4", "sym": "MS_NODEV"}, {"type": "const", "raw": "0x8", "sym": "MS_NOEXEC"}]}]}
```

Verbose rendering: `0xc0ed0000 /* MS_MGC_VAL */|0xf /* MS_RDONLY|MS_NOSUID|MS_NODEV|MS_NOEXEC */`

Hugetlb page sizes in `mmap` flags use a shift expression as a plain string
element:

```json
{"type": "flags", "flags": [{"raw": "0x2", "elems": [{"type": "const", "raw": "0x2", "sym": "MAP_PRIVATE"}]}, {"raw": "0x40000", "elems": [{"type": "const", "raw": "0x40000", "sym": "MAP_HUGETLB"}]}, {"raw": "0x540000", "elems": ["21<<MAP_HUGE_SHIFT"]}]}
```

#### QCMD encoding

The first argument of `quotactl` and `quotactl_fd` (a `QCMD`) is a
two-group flags value: one group for the command, one for the quota type
(a `quota_type` element).

Known command:

```json
{
  "type": "flags",
  "flags": [
    {"raw": "0x800002", "elems": [{"type": "const", "raw": "0x800002", "sym": "Q_QUOTAOFF", "style": "abbrev"}]},
    {"raw": "0x0", "elems": [{"type": "quota_type", "raw": "0", "sym": "USRQUOTA", "style": "abbrev"}]}
  ]
}
```

Unknown command:

```json
{
  "type": "flags",
  "flags": [
    {"raw": "0xbadc0d", "elems": [{"type": "const", "raw": "0xbadc0d", "sym": "Q_???", "style": "abbrev"}]},
    {"raw": "0xed", "elems": [{"type": "quota_type", "raw": "0xed", "sym": "???QUOTA", "style": "abbrev"}]}
  ]
}
```

Traditional output: `QCMD(cmd, quota_type)`, with `cmd` taken from the
command group and `quota_type` from the quota_type group.

#### Bitset values

Some values (e.g., `ICMP_FILTER`) are displayed as bitsets with
space-separated elements in square brackets. These use `type: "bitset"`
rather than `flags`. `elem_type` names the expected element value type. When
more than half the bits are set, strace inverts the listing (`elems` lists
the *missing* members instead) and the value carries `_inverted: true`.
(Traditional output prepends `~` in that case.)

```json
{"type": "bitset", "raw": "0x3d", "elem_type": "const", "elems": [{"type": "const", "raw": "0", "sym": "ICMP_ECHOREPLY"}, {"type": "const", "raw": "3", "sym": "ICMP_DEST_UNREACH"}]}
```

Traditional output: `[ICMP_ECHOREPLY ICMP_DEST_UNREACH]`

Inverted:

```json
{"type": "bitset", "raw": "0x3d", "elem_type": "const", "inverted": true, "elems": [{"type": "const", "raw": "0", "sym": "ICMP_ECHOREPLY"}, {"type": "const", "raw": "3", "sym": "ICMP_DEST_UNREACH"}]}
```

Traditional output: `~[ICMP_ECHOREPLY ICMP_DEST_UNREACH]`

### bitset

Bitmask displayed as set membership in square brackets. `raw` is the original
numeric mask value. `elem_type` names the element type (for example
`"const"` or `"signal"`). `elems` contains the decoded set elements as typed
values. When strace uses inverted rendering, `inverted` is `true` and
`elems` lists the missing elements.

```json
{"type": "bitset", "raw": "0x3d", "elem_type": "const", "elems": [{"type": "const", "raw": "0", "sym": "ICMP_ECHOREPLY"}, {"type": "const", "raw": "3", "sym": "ICMP_DEST_UNREACH"}]}
{"type": "bitset", "raw": "0x3d", "elem_type": "signal", "inverted": true, "elems": [{"type": "signal", "raw": "9", "sym": "SIGKILL"}]}
```

### sigset

Signal set (bitmask of signals). Represented as `type: "bitset"` with
`elem_type: "signal"`. When most signals are set, `inverted` is `true` and
`elems` lists the signals NOT in the set.

```json
{"type": "bitset", "raw": "0x200", "elem_type": "signal", "elems": [{"type": "signal", "raw": "10", "sym": "SIGUSR1"}]}
{"type": "bitset", "raw": "0x1fd", "elem_type": "signal", "inverted": true, "elems": [{"type": "signal", "raw": "9", "sym": "SIGKILL"}]}
```

Traditional: `[USR1]`, `~[KILL]`

### alternatives

A value that has multiple equally-valid representations.  `elems` is an
array of alternative interpretations.  Elements may be bare strings
(e.g., ioctl synonym names) or typed value objects (heterogeneous-type
alternatives).  Traditional output joins the alternatives with ` or `.

Two synonymous ioctl names for the same command number:

```json
{"type": "alternatives", "elems": ["BTRFS_IOC_FILE_EXTENT_SAME", "FIDEDUPERANGE"]}
```

Traditional output: `BTRFS_IOC_FILE_EXTENT_SAME or FIDEDUPERANGE`

64-bit address that is too large for the tracee's `kernel_long_t` —
emitted on `CAN_ARCH_BE_COMPAT_ON_64BIT_KERNEL` so consumers see both the
raw pointer and the decoded value:

```json
{"type": "alternatives", "elems": [
  {"type": "addr", "raw": "0xffffffff00000000"},
  {"type": "string", "value": "/dev/null"}
]}
```

Traditional output: `0xffffffff00000000 or "/dev/null"`

### syscall

Syscall identifier. `name`/`scno` are the user-written syscall (the one
the program actually called and that strace displays). `scno` is always
present.

When `-e inject=...:syscall=X` rewrites the syscall (telling the kernel
to run a different, "pure" syscall instead so the original has no side
effects), the value also carries:

- `"injected": true` -- the syscall identity has been tampered with
- `"injected_name"`/`"injected_scno"` -- the syscall the kernel actually
  executes (the pure replacement, e.g., `getpid`)

```json
{"type": "syscall", "name": "openat", "scno": "257"}
{"type": "syscall", "name": "openat", "scno": "257", "injected": true, "injected_name": "getpid", "injected_scno": "39"}
```

Plain retval/error injection (no `:syscall=...` clause) does **not** set
`injected`/`injected_*` on the syscall value -- strace internally uses
the kernel's invalid-syscall (-1) path to skip the original, but that's
an implementation detail.  In that case only the return value carries
`_injected: true`.

### signal

Signal number with symbolic name.

```json
{"type": "signal", "raw": "9", "sym": "SIGKILL"}
```

### fd

File descriptor. Optional fields depend on `-y` / `-yy`. The `fd_info` field
is a polymorphic object with a `type` discriminator.

```json
{"type": "fd", "raw": "3"}
```

#### fd_info: path (plain file, `-y`)

```json
{"type": "fd", "raw": "3", "fd_info": {"type": "path", "path": "/tmp/file"}}
{"type": "fd", "raw": "3", "fd_info": {"type": "path", "path": "/tmp/file", "deleted": true}}
```

#### fd_info: dev (device, `-yy`)

```json
{"type": "fd", "raw": "3", "fd_info": {"type": "dev", "path": "/dev/null", "kind": "char", "major": "1", "minor": "3"}}
{"type": "fd", "raw": "3", "fd_info": {"type": "dev", "path": "/dev/ptmx", "kind": "char", "major": "5", "minor": "2", "tty_index": "0"}}
```

#### fd_info: socket (interim, `-yy`)

Uses cached string from socketutils. Will be replaced by structured socket
types in Phase 2.

```json
{"type": "fd", "raw": "3", "fd_info": {"type": "socket", "details": "TCP:[127.0.0.1:8080->127.0.0.1:80]"}}
```

Phase 2 will introduce `socket_inet`, `socket_unix`, `socket_netlink`:

```json
{"type": "fd", "raw": "3", "fd_info": {"type": "socket_inet", "protocol": "TCP", "family": "AF_INET", "src_addr": "192.168.1.1", "src_port": "8080", "dst_addr": "192.168.1.2", "dst_port": "80"}}
{"type": "fd", "raw": "3", "fd_info": {"type": "socket_unix", "protocol": "UNIX", "inode": "12345", "peer_inode": "67890", "path": "/tmp/socket.sock"}}
{"type": "fd", "raw": "3", "fd_info": {"type": "socket_netlink", "protocol": "NETLINK", "nl_protocol": "ROUTE", "portid": "0"}}
```

#### fd_info: pidfd (`-yy`)

```json
{"type": "fd", "raw": "3", "fd_info": {"type": "pidfd", "pid": "1234"}}
{"type": "fd", "raw": "3", "fd_info": {"type": "pidfd", "path": "anon_inode:[pidfd]"}}
```

#### fd_info: signalfd (`-yy`)

```json
{"type": "fd", "raw": "3", "fd_info": {"type": "signalfd", "sigmask": "[SIGINT SIGTERM]"}}
{"type": "fd", "raw": "3", "fd_info": {"type": "signalfd", "path": "anon_inode:[signalfd]"}}
```

#### fd_info: eventfd (`-yy`)

```json
{"type": "fd", "raw": "3", "fd_info": {"type": "eventfd", "count": "0", "id": "1", "semaphore": "0"}}
{"type": "fd", "raw": "3", "fd_info": {"type": "eventfd", "path": "anon_inode:[eventfd]"}}
```

### tid, tgid, pgid, sid

Semantic ID types. Used wherever strace knows the value is a task ID, thread
group ID, process group ID, or session ID.

```json
{"type": "tid", "raw": "4567"}
{"type": "tgid", "raw": "4567"}
{"type": "pgid", "raw": "1000"}
{"type": "sid", "raw": "1000"}
```

Pid-type values may carry two optional fields:

- `"strace_pid"` (integer) -- present when strace runs in a PID namespace;
  carries the PID as seen in strace's own namespace. Traditional output:
  `pid /* pid in strace's PID NS */`.
- `"comm"` (string) -- the thread/process name (comm string), present when
  strace has the information available. Traditional output: the name in angle
  brackets after the pid value, e.g., `1234<main>`.

### uid, gid

User and group IDs. `-1` is displayed as unsigned (`4294967295`).

```json
{"type": "uid", "raw": "1000"}
{"type": "gid", "raw": "1000"}
```

### string

Traced string data from tracee memory.

```json
{"type": "string", "value": "hello world", "truncated": false}
```

`truncated` is `true` when the string was cut short by `-s strsize`.

`style` is an optional display hint controlling escaping -- see the
[`style` Display Hint](#the-_style-display-hint) section for the full
reference. For strings: `"non_ascii"` uses hex for bytes > 0x7f.

### bytes

Binary data. Always displayed with `\xHH` hex escaping in traditional output
(quoted, like strings). Used for raw byte arrays such as hardware addresses,
crypto keys, UUIDs, etc.

```json
{"type": "bytes", "value": "\\xde\\xad\\xbe\\xef"}
```

### path

Filesystem path. Formatted the same as `string` in traditional output
(quoted), but semantically distinct.

```json
{"type": "path", "value": "/dev/null"}
```

### sun_path

Unix socket path. Distinct from `path` because it can be abstract (leading
NUL byte, displayed as `@` prefix in traditional output) and may carry
SELinux context.

```json
{"type": "sun_path", "value": "/tmp/socket.sock"}
{"type": "sun_path", "abstract": true, "value": "hidden"}
```

### mac

MAC / hardware address.

```json
{"type": "mac", "raw": "00:11:22:33:44:55"}
```

### uuid

128-bit UUID.

```json
{"type": "uuid", "raw": "550e8400-e29b-41d4-a716-446655440000"}
```

### mode

File permission mode. Has two forms depending on context.

Simple numeric mode (used by `chmod`, `umask`, etc. where there is no file
type):

```json
{"type": "mode", "raw": "0755"}
```

Symbolic mode with file type decomposition (used in `stat`, `mknod`, etc.
where the mode includes `S_IFMT` bits). The `filetype` field is a `const` for
the file type, `special` is an array of special bit names (`S_ISUID`,
`S_ISGID`, `S_ISVTX`), and `perm` is the remaining permission bits in octal:

```json
{"type": "mode", "raw": "0100644", "filetype": {"type": "const", "raw": "0100000", "sym": "S_IFREG"}, "special": [], "perm": "0644"}
{"type": "mode", "raw": "0104755", "filetype": {"type": "const", "raw": "0100000", "sym": "S_IFREG"}, "special": ["S_ISUID"], "perm": "0755"}
```

Traditional output: `S_IFREG|0644` or `S_IFREG|S_ISUID|0755`.

For composite arguments that are encoded as `xlat_bits | mode_bits`, `mode`
remains a separate value type and can appear as an element inside a `flags`
group.

### time_t

Time value. Optional `formatted` field carries the human-readable timestamp
that traditional strace shows in `/* ... */` comments. Following the "no
comments" principle, this is structured data, not a comment.

```json
{"type": "time_t", "raw": "1707840000"}
{"type": "time_t", "raw": "1707840000", "formatted": "2024-02-13T16:00:00+0000"}
```

`formatted` also appears on `struct timespec` and `struct timeval` fields
when strace appends a human-readable time comment. The `formatted` field is
on the struct value itself, not on individual fields:

```json
{"type": "struct", "fields": {"tv_sec": {"type": "time_t", "raw": "1707840000"}, "tv_nsec": {"type": "unsigned", "raw": "0"}}, "formatted": "2024-02-13T16:00:00+0000"}
```

### ticks

Clock tick count. `hz` is the tick frequency used for conversion (from
`sysconf(_SC_CLK_TCK)`). Optional `formatted` field carries the
human-readable seconds representation computed from `raw` and `hz`. Used for
`times()` struct fields and return values.

```json
{"type": "ticks", "raw": "60", "hz": "100"}
{"type": "ticks", "raw": "60", "hz": "100", "formatted": "0.60 s"}
```

As argument/field value: `60 /* 0.60 s */`. As return value: `60 (0.60 s)`.

### timespec_t

A `struct timespec` value with flat `tv_sec` and `tv_nsec` fields (not nested
in a `struct` wrapper). When `tv_nsec` is a sentinel value (`UTIME_NOW` or
`UTIME_OMIT`), the `sym` field carries the symbolic name.

Normal timespec:
```json
{"type": "timespec_t", "tv_sec": 3735928559, "tv_nsec": 4207869677}
```
Traditional: `{tv_sec=3735928559, tv_nsec=4207869677}`

With timestamp formatting:
```json
{"type": "timespec_t", "tv_sec": 1739900000, "tv_nsec": 123456789, "formatted": "2025-02-18T16:53:20.123456789+0000"}
```
Traditional: `{tv_sec=1739900000, tv_nsec=123456789} /* 2025-02-18T16:53:20.123456789+0000 */`

UTIME_NOW:
```json
{"type": "timespec_t", "tv_sec": 3735928559, "tv_nsec": 1073741823, "sym": "UTIME_NOW"}
```
Traditional (abbrev): `UTIME_NOW`
Traditional (verbose): `{tv_sec=3735928559, tv_nsec=1073741823} /* UTIME_NOW */`

UTIME_OMIT:
```json
{"type": "timespec_t", "tv_sec": 3735928559, "tv_nsec": 1073741822, "sym": "UTIME_OMIT"}
```

### timeval_t

A `struct timeval` value with flat `tv_sec` and `tv_usec` fields (not nested
in a `struct` wrapper). Analogous to `timespec_t` but with microsecond
precision.

```json
{"type": "timeval_t", "tv_sec": 1739900000, "tv_usec": 123456}
```

Traditional output: `{tv_sec=1739900000, tv_usec=123456}`

With timestamp formatting:
```json
{"type": "timeval_t", "tv_sec": 1739900000, "tv_usec": 123456, "formatted": "2025-02-18T16:53:20.123456+0000"}
```

### sockaddr

Socket address. Always carries a `family` field (a `const` value naming the
address family); the remaining fields are address-family-specific. The exact
fields emitted for each family follow what strace's traditional output
renders; consult `src/sockaddr.c` for the authoritative list per family.

Example (IPv4):

```json
{"type": "sockaddr", "family": {"type": "const", "raw": "2", "sym": "AF_INET"}, "addr": "127.0.0.1", "port": "8080"}
```

### ip_addr

IP address (IPv4 or IPv6). `family` distinguishes the address family.
Traditional output: `inet_addr("addr")` for IPv4,
`inet_pton(AF_INET6, "addr", &field)` for IPv6.

```json
{"type": "ip_addr", "family": "AF_INET", "addr": "127.0.0.1"}
{"type": "ip_addr", "family": "AF_INET6", "addr": "::1"}
```

### ifindex

Network interface index. Optional `ifname` field present when strace resolves
the interface name. Displayed as `if_nametoindex("lo")` in traditional
output. `ifname` rather than `name` to avoid collision with the
argument-level `name` field.

```json
{"type": "ifindex", "raw": "1", "ifname": "lo"}
{"type": "ifindex", "raw": "3"}
```

### ioctl_op

Ioctl command number. Carries both symbolic name(s) and the full `_IOC`
decomposition (direction, type byte, number, size). Multiple ioctls can share
the same command number (different subsystems may reuse numbers), so when
present, `sym` is an [`alternatives`](#alternatives) value with one element
per synonymous name.

Known ioctl (read+write direction):

```json
{
  "type": "ioctl_op",
  "raw": "0xc0086409",
  "sym": {"type": "alternatives", "elems": ["DRM_IOCTL_GET_CAP"]},
  "dir": {
    "type": "flags",
    "raw": "0x3",
    "flags": [
      {"type": "const", "raw": "0x1", "sym": "_IOC_WRITE"},
      {"type": "const", "raw": "0x2", "sym": "_IOC_READ"}
    ]
  },
  "ioc_type": "0x64",
  "ioc_nr": "0x09",
  "ioc_size": "0x8"
}
```

Known ioctl (no direction):

```json
{
  "type": "ioctl_op",
  "raw": "0x00006602",
  "sym": {"type": "alternatives", "elems": ["FS_IOC_SETFSLABEL"]},
  "dir": {"type": "flags", "flags": [{"raw": "0x0", "elems": [{"type": "const", "raw": "0x0", "sym": "_IOC_NONE"}]}]},
  "ioc_type": "0x66",
  "ioc_nr": "0x02",
  "ioc_size": "0x0"
}
```

Unknown ioctl:

```json
{
  "type": "ioctl_op",
  "raw": "0x4000de00",
  "dir": {
    "type": "flags",
    "flags": [{"raw": "0x1", "elems": [{"type": "const", "raw": "0x1", "sym": "_IOC_WRITE"}]}]
  },
  "ioc_type": "0xde",
  "ioc_nr": "0x00",
  "ioc_size": "0x0"
}
```

| Field      | Type         | Description                                                                                                  |
|------------|--------------|--------------------------------------------------------------------------------------------------------------|
| `type`     | string       | Always `"ioctl_op"`                                                                                          |
| `raw`      | string       | Full 32-bit ioctl number as hex                                                                              |
| `sym`      | alternatives | Optional. `alternatives` value listing the symbolic synonyms (present when known, may have multiple entries) |
| `dir`      | flags        | Direction bits using `_IOC_NONE`/`_IOC_READ`/`_IOC_WRITE` constants. Always present.                         |
| `ioc_type` | string       | Type byte from `_IOC` decomposition, hex                                                                     |
| `ioc_nr`   | string       | Number from `_IOC` decomposition, hex                                                                        |
| `ioc_size` | string       | Size from `_IOC` decomposition, hex                                                                          |

The decomposition is always present, even for known ioctls, so the type
byte and direction are usable without parsing the symbolic name. `dir`
uses the `flags` type -- `_IOC_NONE` (direction 0) appears as a named
constant in the flags array rather than being omitted, because `_IOC_NONE`
is not guaranteed to be 0 on all architectures.

Traditional output: symbolic name if `sym.elems` has one entry; entries
joined with ` or ` if `sym.elems` has multiple entries;
`_IOC(dir_str, ioc_type, ioc_nr, size)` when `sym` is absent.

### uring_restriction_op

`struct io_uring_restriction`'s opcode byte and the union byte that follows
it (`register_op` / `sqe_op` / `sqe_flags`) form a discriminated union: the
interpretation of the second byte depends on the value of the first. This
type aggregates both into a single value so consumers don't need
syscall-specific knowledge to interpret them.

Fields:

- `raw` -- opcode value (hex string)
- `sym` -- opcode symbolic name (always present; ends in `???` when unknown)
- Exactly one of (depending on `raw`):
  - `register_op` -- typed `const` value (when `IORING_RESTRICTION_REGISTER_OP`)
  - `sqe_op` -- typed `const` value (when `IORING_RESTRICTION_SQE_OP`)
  - `sqe_flags` -- typed `flags` value (when `IORING_RESTRICTION_SQE_FLAGS_*`)
  - `op` -- raw hex string (when opcode is unknown; the union byte's
    interpretation is undetermined)

Known opcode + named sub-field:

```json
{
  "type": "uring_restriction_op",
  "raw": "0",
  "sym": "IORING_RESTRICTION_REGISTER_OP",
  "register_op": {"type": "const", "raw": "0", "sym": "IORING_REGISTER_BUFFERS"}
}
```

Traditional output: `opcode=IORING_RESTRICTION_REGISTER_OP, register_op=IORING_REGISTER_BUFFERS`
(the named sub-field appears as a sibling struct field of `opcode`).

Unknown opcode:

```json
{
  "type": "uring_restriction_op",
  "raw": "0x4",
  "sym": "IORING_RESTRICTION_???",
  "op": "0xef"
}
```

Traditional output: `opcode=0x4 /* IORING_RESTRICTION_??? */ /* op: 0xef */`
(the `op` raw value appears as a second comment on the opcode field).

### struct

Named fields. Field order matches traditional strace output but is not
semantically significant. Fields keyed by name.

```json
{
  "type": "struct",
  "fields": {
    "flags": {"type": "flags", "raw": "0x3f", "flags": [{"type": "const", "raw": "0x1f", "sym": "FLAG_A"}, {"type": "const", "raw": "0x20", "sym": "FLAG_B"}]},
    "data": {
      "type": "struct",
      "fields": {
        "profiles": {"type": "flags", "raw": "0x7", "flags": [{"type": "const", "raw": "0x3", "sym": "PROF_X"}, {"type": "const", "raw": "0x4", "sym": "PROF_Y"}]},
        "usage": {"type": "unsigned", "raw": "0"},
        "devid": {"type": "dev", "major": "0", "minor": "1"}
      }
    }
  }
}
```

`truncated` is `true` when strace abbreviated the struct (did not print all
fields).

```json
{
  "type": "struct",
  "fields": {
    "st_mode": {"type": "mode", "raw": "0100644", "filetype": {"type": "const", "raw": "0100000", "sym": "S_IFREG"}, "special": [], "perm": "0644"},
    "st_size": {"type": "decimal", "raw": "1234"}
  },
  "truncated": true
}
```

`rest_unreadable` is `true` when strace successfully read some fields but
could not read the remaining struct data from the tracee's memory (e.g.,
`umoven` failed). Traditional output: `{size=24, ???}`.

```json
{
  "type": "struct",
  "fields": {
    "size": {"type": "unsigned", "raw": "24"}
  },
  "rest_unreadable": true
}
```

`extra_data` is a string field on struct values containing raw bytes beyond
the known struct size (e.g., BPF attr structs that are larger than the
decoder expects). Traditional output: `/* bytes 48..63 */ "\x00\x00..."`.

`decode_error` is a string field on struct values that records why decoding was
aborted partway through (e.g., `"misplaced struct dm_target_spec"` when a
dm-ioctl payload's offsets don't match the declared sizes). The field
documents a non-fatal decoding error; the partial struct contents preceding
it are still valid. Traditional output: `/* error_text */` after the
preceding field.

`reserved_inline` is a boolean meta-field on a struct field value, marking
the field as a reserved byte range positioned *between* named struct fields
(as opposed to a trailing reserved bitmap remainder), which strace surfaced
only because it was unexpectedly non-zero. In traditional output the field
renders inline on the preceding struct field rather than as a normal
`field=value`: when the value also carries a byte range (`bytes`), the trad
form is `/* bytes B0..B1: RAW */`; otherwise `/* field_name: RAW */`.

### array

Array of values. `truncated` is `true` when strace abbreviated the output
(e.g., due to `-s` limit). `fetch_failed_addr` is present when strace could
not read more elements from the tracee's memory; the value is the hex
address where reading stopped.

```json
{
  "type": "array",
  "elems": [
    {"type": "struct", "fields": {...}},
    {"type": "struct", "fields": {...}}
  ],
  "truncated": true
}
```

Array with fetch failure:
```json
{
  "type": "array",
  "elems": [
    {"type": "struct", "fields": {...}},
    {"type": "addr", "raw": null}
  ],
  "fetch_failed_addr": "0x7fff1234"
}
```

Array elements may carry `index` when the array uses explicit indexing
(e.g., sparse arrays, termios cc array). Traditional output: `[idx]=value`.

```json
{
  "type": "array",
  "elems": [
    {"type": "unsigned", "raw": "3", "index": {"type": "const", "raw": "0", "sym": "VINTR"}},
    {"type": "unsigned", "raw": "28", "index": {"type": "const", "raw": "1", "sym": "VQUIT"}}
  ]
}
```

### query_list

Flattened linked list (e.g., io_uring `IORING_REGISTER_PBUF_STATUS` query
entries). In-kernel these are `next_entry`-chained structs; in structured
output they are flattened into a plain array. `truncated_addr` is present
when traversal stopped due to limits.

```json
{
  "type": "query_list",
  "elems": [
    {"type": "struct", "fields": {"buf_group": ..., "nbufs": ...}},
    {"type": "struct", "fields": {"buf_group": ..., "nbufs": ...}}
  ]
}
```

### cpu_set_t

CPU affinity mask. `elems` is an array of CPU numbers (integers) that are set
in the mask.

```json
{"type": "cpu_set_t", "elems": [0, 1, 2, 3, 4, 5, 6, 7]}
```

Rendered as `[0 1 2 3 4 5 6 7]` in traditional output.

### fd_set

File descriptor set (as used by `select`/`pselect6`). `elems` is an array of
fd-typed objects.

```json
{"type": "fd_set", "elems": [
  {"type": "fd", "raw": "3", "path": "/dev/null"},
  {"type": "fd", "raw": "4", "path": "socket:[12345]"}
]}
```

Rendered as `[3</dev/null> 4<socket:[12345]>]` in traditional output.

### pair

Two-element value displayed as `[a, b]`. Used for paired values read from a
pointer (e.g., `pipe2` fd pairs, `socketpair` results).

```json
{"type": "pair", "elems": [{"type": "fd", "raw": "3"}, {"type": "fd", "raw": "4"}]}
```

Traditional output: `[3, 4]`

### dev

Device identifier. `kind` is required and is either `"char"` or `"block"`.

```json
{"type": "dev", "kind": "char", "major": "1", "minor": "3"}
{"type": "dev", "kind": "block", "major": "8", "minor": "1"}
```

### char

Single character value (e.g., SG_IO interface ID byte).

```json
{"type": "char", "raw": "S"}
```

Traditional output: `'S'`

### fract

Fraction with numerator and denominator (e.g., V4L2 frame rates).

```json
{"type": "fract", "numerator": "30", "denominator": "1"}
```

Traditional output: `30/1`

### port_range

Network port range packed in a 32-bit integer. `lo` and `hi` carry the
decoded port bounds.

```json
{"type": "port_range", "raw": "0x00010400", "lo": "1024", "hi": "1"}
```

Traditional output: `0x00010400 /* 1024..1 */`

### ioprio

I/O priority with class and level.

```json
{"type": "ioprio", "raw": "16386", "class": {"type": "const", "raw": "2", "sym": "IOPRIO_CLASS_BE"}, "level": "2"}
```

### wait_status

Wait status value from wait4/waitpid/waitid. The `status` field determines
which additional fields are present.

Exited:
```json
{"type": "wait_status", "raw": "0x2a00", "status": "exited", "exit_code": 42}
```
Traditional: `[{WIFEXITED(s) && WEXITSTATUS(s) == 42}]`

Signaled:
```json
{"type": "wait_status", "raw": "0x4", "status": "signaled", "signal": {"type": "const", "raw": "4", "sym": "SIGUSR1"}}
```
Traditional: `[{WIFSIGNALED(s) && WTERMSIG(s) == SIGUSR1}]`

Signaled with core dump:
```json
{"type": "wait_status", "raw": "0x83", "status": "signaled", "signal": {"type": "const", "raw": "3", "sym": "SIGQUIT"}, "coredump": true}
```
Traditional: `[{WIFSIGNALED(s) && WTERMSIG(s) == SIGQUIT && WCOREDUMP(s)}]`

Stopped:
```json
{"type": "wait_status", "raw": "0x137f", "status": "stopped", "signal": {"type": "const", "raw": "19", "sym": "SIGSTOP"}}
```
Traditional: `[{WIFSTOPPED(s) && WSTOPSIG(s) == SIGSTOP}]`

Continued:
```json
{"type": "wait_status", "raw": "0xffff", "status": "continued"}
```
Traditional: `[{WIFCONTINUED(s)}]`

Optional `ptrace_event` field when ptrace event bits are set in the status:
```json
{"type": "wait_status", "raw": "0x4057f", "status": "stopped", "signal": {"type": "const", "raw": "5", "sym": "SIGTRAP"}, "ptrace_event": {"type": "const", "raw": "4", "sym": "PTRACE_EVENT_EXEC"}}
```

### kernel_version

Packed kernel version decomposed into major, minor, patch. Displayed as
`KERNEL_VERSION(5, 15, 0)` in traditional output.

```json
{"type": "kernel_version", "raw": "332032", "major": "5", "minor": "15", "patch": "0"}
```

### return field

The syscall event's `return` field holds the return value as a typed
value object directly -- its properties (`type`, `raw`, `sym`, ...) sit at
the top level of the `return` object, with no `{"type": "return",
"value": ...}` wrapper. Extra annotations attach as siblings of the
value's own properties inside the same object.

Success:

```json
"return": {"type": "unsigned", "raw": "0"}
```

Fd return:

```json
"return": {"type": "fd", "raw": "3", "fd_info": {"type": "path", "path": "/dev/null"}}
```

Error (errno appears as a sibling of the value's properties):

```json
"return": {"type": "decimal", "raw": "-1", "errno": {"type": "errno", "raw": "14", "sym": "EFAULT", "strerror": "Bad address"}}
```

Injected return value (`_injected: true` as sibling):

```json
"return": {"type": "fd", "raw": "7", "injected": true}
```

Return with symbolic annotation (e.g., `adjtimex` returns a time state,
`sched_getscheduler` returns a scheduler name): the annotation is carried
on the return value itself via `sym`:

```json
"return": {"type": "const", "raw": "5", "sym": "TIME_INS"}
```

Traditional output: `= 5 (TIME_INS)`.

The special "no value available" / "detached mid-syscall" cases use
[`unavailable`](#unavailable) and `detached` typed values directly:

```json
"return": {"type": "unavailable"}
"return": {"type": "detached"}
```

**Legacy `retstr`:** some decoders still emit a `retstr` field as a
sibling on the return object instead of a properly typed return value.

### errno

Error number from a failed syscall. Has `raw` (errno number), `name`
(symbolic name), and `strerror` (human-readable description).

```json
{"type": "errno", "raw": "14", "sym": "EFAULT", "strerror": "Bad address"}
```

### unavailable

Value that could not be obtained (e.g., process died mid-syscall, or syscall
interrupted by signal with ERESTART).

Plain unavailable:

```json
{"type": "unavailable"}
```

Unavailable with restart error (traditional:
`? ERESTART_RESTARTBLOCK (Interrupted by signal)`):

```json
{"type": "unavailable", "error": "ERESTART_RESTARTBLOCK", "strerror": "Interrupted by signal"}
```

## Syscall Event Details

This section covers the deeper mechanics of the syscall event: per-event
fields, how in-out arguments and mid-syscall changes are represented,
split-struct mechanics, error and detached returns, syscall timing, and
syscall injection.

### Event-level fields

`pid` is present on every event.

`entering_pid` is present on the exit event when the PID changed during the
syscall (execve by a non-leader thread). The event's `pid` is the new PID;
`entering_pid` carries the PID the syscall was entered under. In
`jsonl-split` mode, consumers use this to match the entry event (emitted
under `entering_pid`) with the exit event. In `jsonl-merged` mode, where
there is only one event per syscall, it just records the original PID.

`ip` (instruction pointer) is only present when `-i` is used. It is a hex
address string showing where the syscall was invoked from in user space.
Traditional output: `[00007f1234abcdef]` before the syscall name.

`timestamp` is present when `-t`/`-tt`/`-ttt` is used. It is a typed value
object with `type: "timestamp"` wrapping a `type: "time"` value that carries
`precision`, `seconds`, and `nanoseconds` fields.

`relative_timestamp` is present when `-r` is used. It is a typed value
object with `type: "duration"` carrying the elapsed time since the previous
event, with a `precision` field naming the unit.

### In-out arguments and value changes

Some arguments are modified by the kernel during a syscall. Traditional
strace renders these changes in two different shapes depending on whether
the whole argument changed or only a field inside a struct argument:

- **Whole-argument change** -- the argument value is rendered before and
  after, joined by `=>`. For a scalar: `0x100 => 0x200`. For a struct, the
  whole struct is repeated: `{x=0, y=0} => {x=0, y=1}`.
- **Field-level change inside a struct** -- the struct is rendered once,
  and only the changed fields get inline `=>`: `{x=0, y=0 => 1}`.

#### Whole-argument change

The entry event carries the before-value; the exit event carries the
after-value at the same arg index. Two metadata flags mark these arguments:

- `"inout": true` on the entry event's argument -- this argument may be
  written back by the kernel. A forward hint for streaming consumers to hold
  onto the entry value.
- `"changed": true` on the exit event's argument -- only present when the
  value actually differs from the entry value. Absence of `changed` means
  the value is unchanged (or was not re-read by the kernel).

Entry:

```json
{"args": [{"arg": "offset", "type": "addr", "raw": "0x100", "inout": true}]}
```

Exit:

```json
{"args": [{"arg": "offset", "type": "addr", "raw": "0x200", "changed": true}]}
```

`inout`/`changed` can co-occur with `indirect` -- an argument may be both
pointer-dereferenced and kernel-modified (e.g., `[28] => [16]` for addrlen).

#### Field-level change inside a struct

The struct itself appears in both the entry and exit events as a
[`split` struct](#split-structs). The struct as a whole does not carry
`changed`; instead the converter detects fields that appear in both
halves with differing values and renders them as `field=OLD => NEW`
inline within the merged struct. See
[`changed` within `split` structs](#changed-within-split-structs) below
for the full mechanism and example.

### `split` structs

Some structs have their fields delivered partly on entry and partly on exit
(e.g. `recvmsg`'s `msghdr`, where `msg_namelen` is supplied on entry and
the rest on exit). Both halves carry `"split": true`. Each half is
explicitly marked so consumers can identify split structs from either side
without relying on implicit context.

In `jsonl-split` mode, the two halves arrive as separate events. In
`jsonl-merged` mode, they appear as the two elements of the `[entry, exit]`
arg pair. Either way, the logical struct value is the union of fields from
both halves.

### `changed` within `split` structs

When a `split` struct has a field in both the entering and exiting events,
`_changed: true` on the exit field marks the field's value as having changed
from the entering value; without `changed`, the two values are expected
to be identical and the exit value is the live one.

Example: `recvmsg` emits `msg_namelen` (the buffer size) in a `split` struct
on entry, and the full struct on exit. When the kernel returns a different
length:

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
`msg_namelen` is unchanged (no `changed` flag), the traditional form is
just `msg_namelen=36`.

### Error return

Traditional: `openat(AT_FDCWD, NULL, O_RDONLY) = -1 EFAULT (Bad address)`.

```json
{
  "event": "syscall",
  "pid": 1234,
  "syscall": {"type": "syscall", "name": "openat", "scno": "257"},
  "entering": false,
  "args": [],
  "return": {"type": "decimal", "raw": "-1", "errno": {"type": "errno", "raw": "14", "sym": "EFAULT", "strerror": "Bad address"}}
}
```

### No return (process died mid-syscall)

Traditional: `openat(AT_FDCWD, "/dev/null", O_RDONLY) = ?`.

```json
{
  "event": "syscall",
  "pid": 1234,
  "syscall": {"type": "syscall", "name": "openat", "scno": "257"},
  "entering": false,
  "args": [],
  "return": {"type": "unavailable"}
}
```

### Detached mid-syscall

Traditional: `ptrace(PTRACE_TRACEME <detached ...>`.

When strace detaches from a tracee between a syscall's entry stop and its
exit stop, the entry event is emitted normally (no `return` field), and a
synthetic exit event is emitted to mark that no real exit will follow.
The synthetic exit has empty `args` and `"return": {"type": "detached"}`.

The synthetic exit is the per-syscall signal: it tells streaming consumers
to finalize the pending syscall (otherwise the entry event would have no
matching exit) and carries the marker the converter needs to render trad's
`<detached ...>` suffix.  A separate
[`detached` event](#detached-event) follows for the process as a whole.
Both are kept so the syscall-level and process-level signals are explicit
and locally complete, rather than requiring consumers to correlate.

Entry:

```json
{
  "event": "syscall",
  "pid": 1234,
  "syscall": {"type": "syscall", "name": "ptrace", "scno": "101"},
  "entering": true,
  "args": [{"arg": "request", "type": "const", "raw": "0", "sym": "PTRACE_TRACEME"}]
}
```

Synthetic exit:

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

### Interrupted by signal

Traditional: `= ? ERESTART_RESTARTBLOCK (Interrupted by signal)`.

```json
{
  "event": "syscall",
  "pid": 1234,
  "syscall": {"type": "syscall", "name": "clock_nanosleep", "scno": "230"},
  "entering": false,
  "args": [],
  "return": {"type": "unavailable", "error": "ERESTART_RESTARTBLOCK", "strerror": "Interrupted by signal"}
}
```

### Syscall timing (`--syscall-times`)

Traditional: `nanosleep({tv_sec=1, tv_nsec=0}, NULL) = 0 <1.000>`.

The `time` and `time_unit` fields appear at the top level of exit events (not
inside `return`). `time` is the duration as a quoted integer in the given
unit. `time_unit` is one of `"seconds"`, `"milliseconds"`, `"microseconds"`,
`"nanoseconds"`.

```json
{
  "event": "syscall",
  "pid": 1234,
  "syscall": {"type": "syscall", "name": "nanosleep", "scno": "35"},
  "entering": false,
  "args": [null, {"arg": "rem", "type": "addr", "raw": null}],
  "return": {"type": "unsigned", "raw": "0"},
  "time": "1000",
  "time_unit": "milliseconds"
}
```

### Syscall injection (`-e inject`)

When strace injects syscall results, every injected value carries
`"injected": true` -- on individual arguments and on the return object. This
lets consumers know exactly which values are synthetic.

Injected return value only (most common, `-e inject=SET:retval=N`).
Traditional: `openat(AT_FDCWD, "/dev/null", O_RDONLY) = 7 (INJECTED)`.

```json
{
  "event": "syscall",
  "pid": 1234,
  "syscall": {"type": "syscall", "name": "openat", "scno": "257"},
  "entering": false,
  "args": [],
  "return": {"type": "fd", "raw": "7", "injected": true}
}
```

Injected arguments and return value (`-e inject=SET:retval=N:syscall=OTHER`).
Traditional:
`openat(AT_FDCWD, "/dev/null", O_RDONLY) = 7 (INJECTED: args, retval)`.

```json
{
  "event": "syscall",
  "pid": 1234,
  "syscall": {"type": "syscall", "name": "openat", "scno": "257", "injected": true, "injected_name": "getpid", "injected_scno": "39"},
  "entering": false,
  "args": [
    {"arg": "dirfd", "type": "const", "raw": "-100", "sym": "AT_FDCWD", "injected": true},
    {"arg": "pathname", "type": "string", "value": "/dev/null", "injected": true}
  ],
  "return": {"type": "fd", "raw": "7", "injected": true}
}
```

Traditional output appends `(INJECTED)`, `(INJECTED: args)`, or
`(INJECTED: args, retval)` after the return value; the structured form
carries the equivalent information as `_injected: true` on each
injected value (the return and/or individual args), and no separate
`inject`/marker field on the return object.

### Delayed injection (`-e inject=SET:delay_enter=N`)

Traditional: `(DELAYED)` after the return value.

Signaled at the *event level*, not inside `return`: the syscall event
gets `"delayed": true` (boolean) and, when the configured delay is
known, `"delayed_by"` (integer-as-string) plus `"delayed_by_unit"` (one
of `"seconds"`/`"milliseconds"`/`"microseconds"`/`"nanoseconds"`).

```json
{
  "event": "syscall",
  "pid": 1234,
  "entering": false,
  "return": {"type": "unsigned", "raw": "0"},
  "delayed": true,
  "delayed_by": "1000000",
  "delayed_by_unit": "microseconds"
}
```

## Complete Example: sysinfo

Traditional output:

```
sysinfo({uptime=6252125, loads=[643424, 318720, 119712], totalram=1081646206976,
  freeram=736271814656, sharedram=61651206144, bufferram=1085440,
  totalswap=1920383406080, freeswap=1920204382208, procs=2612, totalhigh=0,
  freehigh=0, mem_unit=1}) = 0
```

Structured JSONL (entry has no args since all fields are out-parameters):

```json
{"event": "syscall", "pid": 1234, "syscall": {"type": "syscall", "name": "sysinfo", "scno": "99"}, "entering": true, "args": []}
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

## Special Cases

### dirfd (AT_FDCWD)

The `*at()` family of syscalls (`openat`, `fstatat`, etc.) take a `dirfd`
argument that can be either a real file descriptor or the sentinel
`AT_FDCWD`. There is no separate `dirfd` type. Instead, the printer emits
`const` for `AT_FDCWD` and `fd` for a real fd. The consumer checks `type` on
the arg value:

```json
{"arg": "dirfd", "type": "const", "raw": "-100", "sym": "AT_FDCWD"}
{"arg": "dirfd", "type": "fd", "raw": "3", "fd_info": {"type": "path", "path": "/srv/strace"}}
```

This follows the same pattern as UTIME_NOW (below).

### UTIME_NOW / UTIME_OMIT

The `utimensat` and `futimens` syscalls accept `struct timespec` arrays where
`tv_nsec` can be the sentinel value `UTIME_NOW` (1073741823) or `UTIME_OMIT`
(1073741822). These are handled by the `timespec_t` type (see above) with a
`sym` field.

## Data-Fetching Options and Their Effect on Structured Output

Structured output does not imply any extra data-fetching. The following
options control what data strace collects; structured output simply formats
whatever was collected:

| Option           | Effect                            | Structured output                     |
|------------------|-----------------------------------|---------------------------------------|
| `-y`             | Resolve fd paths                  | `fd_info` with `type: "path"` on `fd` |
| `-yy`            | Detailed fd info (device, socket) | `fd_info` with `type: "dev"`, etc.    |
| `-k`             | Stack traces                      | TBD                                   |
| `-s N`           | String truncation limit           | `truncated` field on `string` values |
| `-e verbose=set` | Decode structures in detail       | More fields in `struct` values        |

## The `style` Display Hint

The `style` field is an optional display hint on value objects. It does
not change the meaning of the data, only how traditional strace formats
it for human-readable output. The underlying semantic value is fully
described by the other fields, so the hint may be safely ignored when
only the data matters.

### `style` on string/path values

Controls byte escaping for non-printable characters:

| Value         | Meaning                                       | C origin              |
|---------------|-----------------------------------------------|-----------------------|
| (absent)      | Default: octal `\NNN`, named `\n` etc.        | Normal string quoting |
| `"non_ascii"` | Hex for bytes > 0x7f, named for control chars | `-x` mode per-value   |

Binary data that needs `\xHH` escaping uses `"type": "bytes"` instead of
`style` -- see the [bytes](#bytes) type.

### `style` on struct values (timestamp)

When `style` is `"timestamp"`, the struct represents a time value that
traditional strace annotates with a `/* ISO-8601 */` comment computed
from the struct's `tv_sec` + `tv_nsec`/`tv_usec` fields.

Only specific C printer functions emit this -- it is not added to all
timespec/timeval structs. For example, `utimensat` time args get it, but
`adjtimex` internal fields do not.

```json
{"type": "struct", "style": "timestamp", "fields": {"tv_sec": ..., "tv_nsec": ...}}
```

### `style` on mode values

When `style` is `"octal"`, the raw value should be formatted with a leading
zero (e.g., `0755` not `493` or `0x1ed`). This is the standard for mode
values.

```json
{"type": "mode", "style": "octal", "raw": "0755"}
```

### `style` on const/flags values (xlat verbosity)

When `style` is `"verbose"`, the value should be formatted in verbose xlat
style (`raw /* SYMBOLIC */`) regardless of the global `-X` setting. Some
syscalls (e.g., `syslog`) hardcode verbose xlat style in their C printers.

```json
{"type": "const", "style": "verbose", "raw": "2", "sym": "SYSLOG_ACTION_READ"}
```

When `style` is `"abbrev"`, the value should always use abbreviated format
(`SYMBOLIC`) regardless of the global `-X` setting.

Without `style`, the global `-X` setting selects between raw, abbrev,
and verbose formatting.

### `style: "bitset"` on flags values

When `style` is `"bitset"`, the flags value should be rendered as a
space-separated set in square brackets (like the `bitset` type), rather than
pipe-separated flags. Used by ICMP_FILTER and similar socket options.

```json
{"type": "flags", "style": "bitset", "flags": [{"raw": "0x3d", "elems": [...]}]}
```

Traditional output: `[ELEM1 ELEM2]` (space-separated, square brackets).

## Presentation Options (`-X`) and Structured Output

The `-X raw`, `-X verbose`, `-X abbrev` options only affect traditional text
output formatting. Structured output always emits both `raw` and `sym`
fields when available, since both are already computed internally at no
extra cost.

Some syscalls override the global `-X` setting for specific arguments (e.g.,
`syslog` forces verbose style). These per-value overrides are communicated
via `_style: "verbose"` on the affected value objects, so the override is
intrinsic to the value rather than something a reader has to special-case
per syscall.

## Interim and Future Work

This section groups non-schema-stable notes that are intentionally kept
separate from the normative schema definitions above.

- **interim**: `fd_info.type: "socket"` appears as a legacy string form from
  socketutils:
  `{"type": "fd", "raw": "3", "fd_info": {"type": "socket", "details": "TCP:[127.0.0.1:8080->127.0.0.1:80]"}}`
- **Phase 2**: structured socket variants are planned: `socket_inet`,
  `socket_unix`, `socket_netlink`.
- **TBD**: stack trace schema details for `-k`.
- **Legacy `retstr`**: some decoders still emit a `retstr` field on the
  return object instead of a properly typed return value.
- **Future extensions**: additional value types may be added as needed,
  following the same pattern -- a `"type"` discriminator plus type-specific
  fields.
