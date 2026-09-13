#!/usr/bin/env bash
#
# renlib correctness gate. Runs every mode the manual describes and reports
# pass or fail.
#
# Each run is captured to a file first, and the program's status is read
# before the text is examined. A nonzero status fails the case on its own,
# so a crash cannot pass as a match. The older script piped the program
# straight into grep, and a pipeline reports the status of its last command.
#
#       ./run_tests.sh
#
# Build bench first (make). The script exits nonzero if any check fails.

set -uo pipefail

bin="./bench"
if [ ! -x "$bin" ] && [ -x "$bin.exe" ]; then
    bin="$bin.exe"
fi

if [ ! -x "$bin" ]; then
    echo "run_tests.sh: $bin not found. Build it first: make" >&2
    exit 1
fi

out="./.bench.out"
trap 'rm -f "$out"' EXIT

failures=0
total=0

# expect - the run must exit 0 and its output must match the pattern.
expect() {
    local name="$1"
    local pattern="$2"
    shift 2
    total=$((total + 1))

    "$bin" "$@" > "$out" 2>&1
    local status=$?

    if [ "$status" -ne 0 ]; then
        echo "FAIL  $name (the program exited with status $status)"
        sed -n '1,10p' "$out" | sed 's/^/      /'
        failures=$((failures + 1))
        return
    fi

    if grep -qE "$pattern" "$out"; then
        echo "ok    $name"
    else
        echo "FAIL  $name (no line matched: $pattern)"
        sed -n '1,10p' "$out" | sed 's/^/      /'
        failures=$((failures + 1))
    fi
}

# refuse - the run must exit with the expected nonzero status.
refuse() {
    local name="$1"
    local want="$2"
    shift 2
    total=$((total + 1))

    "$bin" "$@" > "$out" 2>&1
    local status=$?

    if [ "$status" -eq "$want" ]; then
        echo "ok    $name refused (status $status)"
    else
        echo "FAIL  $name (status $status, wanted $want)"
        sed -n '1,6p' "$out" | sed 's/^/      /'
        failures=$((failures + 1))
    fi
}

# --- Sorting ---------------------------------------------------------------
for file in tests/*.txt; do
    expect "sort $file" "Match: +YES" --sort "$file"
done

# --- Searching -------------------------------------------------------------
for file in tests/*.txt; do
    expect "search $file" "bsearch_asm: all probes correct" --search "$file"
done

# --- Euclid ----------------------------------------------------------------
expect "gcd 1071 462" "gcd_asm\(1071, 462\) = 21" --gcd 1071 462
expect "gcd 1071 462 depth" "Recursion depth reached: 4" --gcd 1071 462
expect "gcd 0 42" "gcd_asm\(0, 42\) = 42" --gcd 0 42
expect "gcd 42 0" "gcd_asm\(42, 0\) = 42" --gcd 42 0
expect "gcd 0 0" "gcd_asm\(0, 0\) = 0" --gcd 0 0

# --- Reentrancy and the calling convention ---------------------------------
expect "reentrancy" "Reentrancy: +PASS" --reentrancy
expect "hostile" "Hostile caller: +PASS" --hostile

# --- Arguments and files that must be refused ------------------------------
refuse "--gcd -1 5" 2 --gcd -1 5
refuse "--gcd 5x 7" 2 --gcd 5x 7
refuse "--gcd 5 7 9" 2 --gcd 5 7 9
refuse "--gcd 5" 2 --gcd 5
refuse "--sort with no file" 2 --sort
refuse "--sort missing file" 1 --sort tests/does-not-exist.txt
refuse "--search missing file" 1 --search tests/does-not-exist.txt

# A file with a valid prefix and then a malformed token is refused whole.
for bad in tests/bad/*.txt; do
    refuse "sort $bad" 1 --sort "$bad"
    refuse "search $bad" 1 --search "$bad"
done

echo
if [ "$failures" -eq 0 ]; then
    echo "All $total checks passed."
    exit 0
else
    echo "$failures of $total checks failed."
    exit 1
fi
