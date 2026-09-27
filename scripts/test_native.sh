#!/bin/bash
# Compare separate unpatched/patched processes; keep local results in build/.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/common.sh"
if [[ ${1:-} == --help ]]; then printf '%s\n' 'Usage: scripts/test_native.sh'; exit 0; fi
[[ $# == 0 ]] || die 'Usage: scripts/test_native.sh'
require_arm64
library="$BUILD/libaccelerate_rounding.dylib"
[[ -f $library ]] || die 'Run scripts/build.sh first (optionally --native-only).'
for name in witness stress scope policy; do [[ -x $BUILD/$name ]] || die "Missing executable: $BUILD/$name"; done
# Remove stale success summaries before running. Publish JSON only on success.
rm -f "$BUILD/native-results.json"
summary=$(mktemp "$BUILD/native-results.XXXXXX")
trap 'rm -f -- "$summary"' EXIT
printf '[\n' > "$summary"
separator=
for mode in baseline patched; do
    for name in witness stress; do
        label="$name-$mode"
        status=0
        (
            unset DYLD_INSERT_LIBRARIES VECLIB_MAXIMUM_THREADS
            export ACCELERATE_ROUNDING_AUDIT=1
            if [[ $mode == patched ]]; then export DYLD_INSERT_LIBRARIES="$library"; fi
            # Direct exec avoids a protected /usr/bin/env stripping DYLD_*.
            exec "$BUILD/$name"
        ) > "$BUILD/$label.log" 2>&1 || status=$?
        [[ $status == 0 || $status == 1 ]] || die "Test did not complete: $label (exit $status)"
        if [[ $name == witness ]]; then sentinel=WITNESS_COMPLETE; else sentinel=STRESS_COMPLETE; fi
        grep -q "$sentinel" "$BUILD/$label.log" || die "Missing completion marker: $label"
        patched=false
        if [[ $mode == patched ]]; then
            patched=true
            [[ $status == 0 ]] || die "Patched enclosure test failed: $label"
            awk '
                /foreign_thread_callbacks=/ {
                    for (i=1;i<=NF;i++) {
                        split($i, a, "=")
                        if (a[1]=="foreign_thread_callbacks" && a[2]+0>0) foreign=1
                        if (a[1]=="peak_overlapping_callbacks" && a[2]+0>=2) overlap=1
                    }
                }
                END { exit !(foreign && overlap) }
            ' "$BUILD/$label.log" || die "Parallel BLAS callback coverage unverified: $label"
        fi
        printf '%s  {"test":"%s","patched":%s,"exit_code":%s}' "$separator" "$name" "$patched" "$status" >> "$summary"
        separator=$',\n'
        if [[ $status == 0 ]]; then result=PASS; else result='rounding failure reproduced'; fi
        printf '%s %s\n' "$label" "$result"
    done
done
(
    export ACCELERATE_ROUNDING_AUDIT=1 DYLD_INSERT_LIBRARIES="$library"
    exec "$BUILD/scope"
) > "$BUILD/scope-patched.log" 2>&1 || die 'BLAS-only interposition scope test failed.'
grep -q 'SCOPE_COMPLETE passed=1' "$BUILD/scope-patched.log" || die 'Missing scope completion marker.'
printf '%s  {"test":"scope","patched":true,"exit_code":0}' "$separator" >> "$summary"
"$BUILD/policy" > "$BUILD/policy.log" 2>&1 || die 'Automatic CPU-selection policy test failed.'
grep -q 'POLICY_COMPLETE cases=128 passed=1' "$BUILD/policy.log" || die 'Missing policy completion marker.'
printf '%s  {"test":"policy","patched":true,"exit_code":0}' "$separator" >> "$summary"
printf '\n]\n' >> "$summary"
mv "$summary" "$BUILD/native-results.json"
printf 'NATIVE_TESTS_COMPLETE\n'
