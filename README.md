<!--no-pdf-->
# CMSC 131 Lab 2 Starter

[![lab2-checks](https://github.com/WhiteLicorice/cmsc-131-lab2-starter/actions/workflows/test.yml/badge.svg)](https://github.com/WhiteLicorice/cmsc-131-lab2-starter/actions/workflows/test.yml)

A C-callable library of sort, search, and utility routines in assembly. The manual is the assignment. This file is the repository's own notes.

## Layout

```text
Makefile       platform preamble and build rules
bench.c        provided: file I/O, timing, the C-side comparison, the hostile caller
cdecl.h        provided: the calling-convention macros
LICENSE        provided
sort.asm       yours
search.asm     yours
util.asm       yours
run_tests.sh   provided: the correctness gate
tests/         provided: the test corpus
```

## What to Run

On Windows, run these commands in Git Bash, the shell from Block 1. In that
shell, `make` is your alias for `mingw32-make`. On Linux, use your terminal.

```bash
make
make check
```

`make` builds `bench`. `make check` builds, then runs `./run_tests.sh`,
which reports each check and exits nonzero when any of them fail. Each run
is captured first. Its exit status is read before its text is examined,
so a program that prints the right line and then crashes still fails.

## Reading a First Run

The assembly files ship as stubs that assemble and link as-is, so the build
works before you write any code. Right now they do nothing useful, and 19
of the 36 checks fail. The seventeen that pass prove little. Four are files
that arrive empty, single-element, or already sorted. `gcd(0, 0)` expects 0.
The hostile caller cannot be upset by a stub that touches no register. The
rest are argument and file refusals that the harness performs before any
assembly runs. That red run is the correct starting state for a starter. The
badge stays red until you implement the routines.

The provided files are fixtures. The grader compares your fork against the
starter. An edit to `bench.c`, `Makefile`, `run_tests.sh`, or a `tests/`
file appears as a diff in the open.

## Documentation

The three sections at the end of this file are yours. Complete Design Notes
and Subsystem Ownership before the Week 1 progress report. Complete Quirks
and Issues before the Week 3 progress report. Each section says what it
needs. Leave the rest of this file as it is.

---

## Design Notes

Complete this section before the Week 1 progress report and finish it by
Week 3. The syllabus asks for problem analysis, a solution architecture,
and an estimated timeline. The manual's Deliverables section lists what
this activity adds. Keep each part short. Update it when the plan changes.

### Problem analysis

The four routines `bench.c` expects and what each one returns. Which
registers you preserve and why.

| Routine | Arguments | Returns |
|---|---|---|
| `qsort_asm` | `int *arr, int n` | nothing; sorts `arr` in place, ascending |
| `bsearch_asm` | `int *arr, int n, int key` | index of `key`, or -1 if absent |
| `gcd_asm` | `int a, int b` | gcd of `a` and `b` |
| `sum_range_asm` | `int lo, int hi` | sum of `lo..hi`, or 0 if `lo > hi` |

- All four are recursive.
- None calls the C standard library. The only call into C is
  `sum_range_asm` calling `bench_callback`.
- `gcd_asm` also sets the global `gcd_depth` to the deepest number of gcd
  calls on the stack, counting the first as 1. `bench.c` reads it after
  the call returns, so we keep the peak, not the current level.
- `sum_range_asm` calls `bench_callback` once per call from C, never from
  its recursion. `bench` uses it to start a second call mid-flight and
  checks that both results are right.

#### The cdecl convention

| Rule | Detail |
|---|---|
| Arguments | Pushed right to left by the caller. The first is at `[ebp+8]`. |
| Cleanup | The caller removes the arguments after the call. |
| Return value | In `eax`. |
| Caller-saved | `eax`, `ecx`, `edx`. We may clobber them. |
| Callee-saved | `ebx`, `esi`, `edi`, `ebp`. We push and pop any we use. `bench --hostile` checks this. |

### Solution architecture

A stack frame diagram for one recursive call of `qsort_asm`, showing where
the arguments, the return address, saved `ebp`, and the saved registers
sit. Why `sum_range_asm` is reentrant.

#### Why recursive, and why three files

- **Recursive:** the activity tests how we build stack frames. Each call
  keeps its own `lo` and `hi` in its frame. A loop with a manual stack
  would skip that.
- **Three files:** `sort.asm`, `search.asm`, and `util.asm`, one per
  subsystem. Each of us owns one file, so we don't step on each other and
  the history shows who did what.

#### `qsort_rec` stack frame

The recursion happens in `qsort_rec`, the routine `qsort_asm` calls. One
frame:

```
  hi               [ebp+16]   pushed by caller
  lo               [ebp+12]
  arr              [ebp+8]
  return address   [ebp+4]    pushed by call
  caller's ebp     [ebp]      pushed by enter
  caller's ebx     [ebp-4]    pushed by us
```

Each recursive call adds a new frame below with its own `lo` and `hi`.
The caller removes the arguments with `add esp, 12`.

#### Preserved registers

| Routine | Saves | Why |
|---|---|---|
| `qsort_asm` | none | Uses only `eax`, which is caller-saved |
| `qsort_rec` | `ebx` | Holds `p` across both recursive calls |
| `partition` | `ebx`, `esi`, `edi` | Hold `i`, `arr`, and the pivot |
| `bsearch_asm`, `bsearch_rec` | none | Use only `eax`, `ecx`, `edx` |
| `gcd_asm`, `gcd_rec` | none | Use only `eax`, `ecx`, `edx` |
| `sum_range_asm`, `sum_range_rec` | none | Use only `eax`, `ecx`, `edx` |

Every routine also saves `ebp` with `enter` and restores it with `leave`.

#### `sum_range_asm` and reentrancy

How it works:

1. `sum_range_asm` calls `bench_callback` once.
2. It then calls the internal helper `sum_range_rec(lo, hi)`, which never
   calls back.
3. The helper returns 0 for an empty range and `lo` for a single value.
   Otherwise it returns `lo` plus the recursive sum of `lo + 1..hi`.
   Stopping at equality also avoids incrementing `INT_MAX` at the end of
   a range.

Why it is reentrant:

- Each call keeps its arguments and pending additions in its own stack
  frames, so a callback can start another sum without changing the first
  call's state.
- Neither routine uses mutable `.data` or `.bss` storage, and neither
  touches `gcd_depth`.
- Both preserve the callee-saved registers and return the sum in `eax`.

What a non-reentrant version would look like: a shared accumulator in
`.bss`, reset at entry, with each value added to it. In `bench
--reentrancy`:

1. The outer `sum_range(1, 1000)` resets the accumulator, then calls back.
2. The nested `sum_range(1, 100)` runs to completion and leaves 5050 in it.
3. The outer call adds its own 500500 and returns 505550, not 500500.

With stack frames, the nested result stays separate and both sums are
correct.

### Measurements

The ratio `bench` reports on `random1000.txt`, `sorted1000.txt`,
`reverse1000.txt`, `duplicates.txt`, and `identical.txt`. Your worst case
and its cause. One improvement and its estimated effect.

| File | Integers | Ratio |
|---|---|---|
| `random1000.txt` | 1000 | 0.40x |
| `sorted1000.txt` | 1000 | 6.07x |
| `reverse1000.txt` | 1000 | 3.10x |
| `duplicates.txt` | 500 | 1.06x |
| `identical.txt` | 100 | 1.49x |

The ratio is assembly time divided by C `qsort` time. Below 1 means the
assembly is faster, above 1 means it is slower.

How we measured:

- November 2, 2026, on Linux x86_64 with an Intel Core i5-12450H.
- The default Makefile build (32-bit, no C optimization flag).
- Each entry is the median of five `./bench --sort tests/<file>`
  invocations. Each invocation times 200 sorts per implementation,
  including copying the input before each sort.
- All 25 invocations reported `Match: YES`.
- Timings vary between runs and machines.

#### Worst case: `sorted1000.txt`

- With Lomuto partition and the last element as pivot, an ascending array
  always picks the largest value as pivot.
- Each partition leaves subarrays of sizes n - 1 and 0, so the work is
  999 + 998 + ... + 1 = 499500 comparisons for 1000 elements: O(n²) time
  and O(n) recursion depth.
- Reverse-sorted and identical inputs also produce these unbalanced
  partitions. On identical input, every value passes the `<= pivot` test.
  Their lower measured ratios do not change that worst-case complexity,
  and the identical fixture is also much smaller.

#### Improvement: median-of-three pivot

- Choose the median of the first, middle, and last values, move it to the
  last position, then partition.
- For sorted distinct input, this gives roughly balanced splits and
  O(n log n) work.
- **Estimated effect:** at n = 1000, on the order of 10000 comparisons
  instead of 499500, roughly 50 times fewer. This estimates comparisons,
  not a measured runtime speedup; pivot selection and other costs still
  matter.
- It would not fix identical input with the current two-way partition.
- This is a proposal only. The implementation keeps the manual's
  last-element pivot.

### Timeline

One line per week. Name the subsystem each week finishes and the member
who owns it.

| Week | Goal | Owner |
|---|---|---|
| 1 | Design Notes, ownership, build runs on the stubs | All of us |
| 2 | Sorting and searching pass their checks | @Romyr05, @andrianllmm |
| 3 | `sum_range_asm`, measurements, quirks and issues | @dejely |
| 4 | Defense | All of us |

## Subsystem Ownership

Complete this section before the Week 1 progress report. The manual lists
the three subsystems. Each member owns one. In a group of four, two members
share one. The commit history must agree with this table.

| Subsystem | Owner |
|---|---|
| Sorting (`qsort_asm` and its partition) | @Romyr05 |
| Searching (`bsearch_asm`, `gcd_asm`) | @andrianllmm |
| Convention and measurement (`sum_range_asm`, the benchmarks) | @dejely |

## Quirks and Issues

Complete this section before the Week 3 progress report. The syllabus asks
for documentation of quirks and issues with the complete implementation.
One entry per item. State what happens, what causes it, and what the group
did about it.

### Known issues

- **`gcd_asm` is not reentrant.**
  - Cause: `gcd_depth` is one shared `.bss` int that every call resets
    and every level raises, so two overlapping calls would mix their
    depths. The returned gcd is still correct, since it lives in
    registers and stack frames.
  - What we did: kept it, because `bench.c` requires the global.
- **`gcd_asm` is undefined for negative operands.**
  - Cause: `idiv` gives a negative remainder for a negative dividend, so
    the result can be negative, and `INT_MIN` with `-1` raises a divide
    fault.
  - What we did: no check, because `bench` refuses negative operands
    before calling.
- **`qsort_asm` overflows the stack on large sorted input.**
  - Cause: sorted input recurses about once per element, and each
    `qsort_rec` frame takes 24 bytes. A 400000-element ascending file
    faults with SIGSEGV under the default 8 MB stack.
  - What we did: left it, because the fixtures stop at 1000 elements.
    Recursing into the smaller side first, or a median-of-three pivot,
    would bound the depth.
- **`sum_range_asm` overflows the stack on wide ranges.**
  - Cause: it recurses once per value, 16 bytes a frame, so
    `sum_range(-1000000, 1000000)` faults although its sum, 0, fits.
  - What we did: left it, because `bench` only asks for short ranges.
- **A sum that does not fit 32 bits wraps around silently.**
  - Cause: `add` does not check for overflow.
  - What we did: no check, because the manual leaves it undefined.

### Quirks

- **On duplicates, `bsearch_asm` returns whichever matching index the
  halving lands on, not the first.** `bench` accepts any index holding
  the key, so we left it.
- **`gcd_depth` depends on argument order.** `gcd(462, 1071)` reaches 5,
  one more than `gcd(1071, 462)`, because the first step only swaps the
  operands. `gcd(a, 0)` and `gcd(0, 0)` reach 1.
- **The midpoint uses `shr`, not `sar`.** That is safe because `hi - lo`
  is never negative once `lo > hi` has been ruled out.
- **An empty array gives `hi = -1`.** The first call returns -1 without
  reading memory.
- **`partition` swaps `arr[i]` with `arr[j]` even when `i == j`.** That
  swap is a no-op, so we skipped the extra compare.
- **`sum_range_asm` calls `bench_callback` even for an empty range.** It
  then returns 0. `bench` counts one callback per C call.
