<!--no-pdf-->
# CMSC 131 Lab 2 Starter

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

```bash
make
make check
```

`make` builds `bench`. `make check` builds, then runs `./run_tests.sh`,
which reports each check and exits nonzero when any of them fail.

## Reading a First Run

The assembly files ship as stubs that assemble and link as-is, so the build
works before any code is written. Right now they do nothing useful, and 16
of the 21 checks fail. The five that pass are the four files that arrive
already sorted and the hostile caller, which a stub that touches no
register can't upset. That red run is the correct starting state for a
starter, and the badge stays red until the routines are implemented.

The provided files are fixtures. The grader compares your fork against the
starter, so an edited `bench.c`, `Makefile`, `run_tests.sh`, or `tests/`
file shows up as a diff in the open.
