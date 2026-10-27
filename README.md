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

`bench.c` calls four routines. Each one is recursive and calls nothing in
the C standard library.

| Routine | Arguments | Returns |
|---|---|---|
| `qsort_asm` | `int *arr, int n` | nothing. Sorts `arr` in place, ascending. |
| `bsearch_asm` | `int *arr, int n, int key` | the index of `key` in the sorted `arr`, or -1. |
| `gcd_asm` | `int a, int b` | the greatest common divisor of `a` and `b`. |
| `sum_range_asm` | `int lo, int hi` | the sum of `lo..hi`, or 0 when `lo > hi`. |

`gcd_asm` also writes the exported `.bss` int `gcd_depth`. It holds the
number of `gcd` calls on the stack at the deepest point of the most recent
call, with the first call as 1. `gcd(1071, 462)` reaches 4. `bench.c` reads
it after the call returns, so it must keep the peak, not the current level.

`sum_range_asm` calls the C function `bench_callback(void)` once per call
from C. Its internal recursion does not call it. `bench` uses the callback
to start a second `sum_range_asm` before the first returns, and counts the
calls.

cdecl, as each C-facing routine obeys it:

- The caller pushes arguments right to left, so the first argument sits at
  `[ebp+8]`, the second at `[ebp+12]`, the third at `[ebp+16]`.
- The caller removes its arguments after the call returns. Each routine does
  `add esp, 4*k` after every call it makes.
- The result returns in `eax`.
- `eax`, `ecx`, and `edx` are caller-saved. A routine can change them.
- `ebx`, `esi`, `edi`, and `ebp` are callee-saved. A routine that uses them
  pushes them in its prologue and pops them before `ret`. The compiled C
  caller keeps live values there, and `bench`'s hostile caller checks them.

### Solution architecture

A stack frame diagram for one recursive call of `qsort_asm`, showing where
the arguments, the return address, saved `ebp`, and the saved registers
sit. Why `sum_range_asm` is reentrant.

The manual requires recursion for the sort and the search. Each recursive
call gets its own stack frame, so its `lo` and `hi` live on the stack, not
in shared storage. The activity tests that frame discipline. An iterative
version with an explicit stack array avoids it, so it does not satisfy the
activity.

The routines are in three modules, one per subsystem: `sort.asm`,
`search.asm`, and `util.asm`. Each member owns one file, so the commit
history shows who did each part, and work in one file does not conflict
with work in another. Each module exports only the names `bench.c` uses and
keeps its internal recursive helper local.

### Measurements

The ratio `bench` reports on `random1000.txt`, `sorted1000.txt`,
`reverse1000.txt`, `duplicates.txt`, and `identical.txt`. Your worst case
and its cause. One improvement and its estimated effect.

| File | Ratio |
|---|---|
| `random1000.txt` | |
| `sorted1000.txt` | |
| `reverse1000.txt` | |
| `duplicates.txt` | |
| `identical.txt` | |

### Timeline

One line per week. Name the subsystem each week finishes and the member
who owns it.

| Week | Goal | Owner |
|---|---|---|
| 1 | Design Notes and ownership. `make && make check` runs on the stubs (17/36). | @dejely |
| 2 | Sorting and Searching. `qsort_asm`, `bsearch_asm`, and `gcd_asm` pass their checks. | @Romyr05, @andrianllmm |
| 3 | Convention and measurement. Reentrant `sum_range_asm`, Measurements, Quirks and Issues. | @dejely |
| 4 | Defense | All members |

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

- 

### Quirks

- 
