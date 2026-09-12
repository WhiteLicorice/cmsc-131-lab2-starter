#!/usr/bin/env bash
#
# renlib correctness gate. Runs every mode the manual describes and reports
# pass or fail. Each sort must report "Match: YES". Every search probe must
# be correct. gcd must be 21 at depth 4. The reentrancy demo and the
# hostile caller must pass.
#
#       ./run_tests.sh
#
# Build bench first (make). The script exits nonzero if any check fails.

set -u

bin="./bench"
if [ ! -x "$bin" ] && [ -x "$bin.exe" ]; then
    bin="$bin.exe"
fi

if [ ! -x "$bin" ]; then
    echo "run_tests.sh: $bin not found. Build it first: make" >&2
    exit 1
fi

failures=0

check_sort() {
    local file="$1"
    if "$bin" --sort "$file" | grep -qE "Match: +YES"; then
        echo "ok    sort $file"
    else
        echo "FAIL  sort $file"
        failures=$((failures + 1))
    fi
}

for file in tests/*.txt; do
    check_sort "$file"
done

for file in tests/*.txt; do
    if "$bin" --search "$file" | grep -q "bsearch_asm: all probes correct"; then
        echo "ok    search $file"
    else
        echo "FAIL  search $file"
        failures=$((failures + 1))
    fi
done

gcd_out="$("$bin" --gcd 1071 462)"
if echo "$gcd_out" | grep -q "gcd_asm(1071, 462) = 21" \
   && echo "$gcd_out" | grep -q "Recursion depth reached: 4"; then
    echo "ok    gcd"
else
    echo "FAIL  gcd"
    failures=$((failures + 1))
fi

if "$bin" --reentrancy | grep -q "Reentrancy:  PASS"; then
    echo "ok    reentrancy"
else
    echo "FAIL  reentrancy"
    failures=$((failures + 1))
fi

if "$bin" --hostile | grep -q "Hostile caller: PASS"; then
    echo "ok    hostile"
else
    echo "FAIL  hostile"
    failures=$((failures + 1))
fi

echo
if [ "$failures" -eq 0 ]; then
    echo "All checks passed."
    exit 0
else
    echo "$failures check(s) failed."
    exit 1
fi
