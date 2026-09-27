#!/bin/bash
# Build locally with Apple's compiler and SDK; no installation or downloads.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/common.sh"
matlab= cc= sdk= native_only=0
while [[ $# -gt 0 ]]; do
    case $1 in
        --matlab) require_value "$@"; matlab=$2; shift 2 ;;
        --cc) require_value "$@"; cc=$2; shift 2 ;;
        --sdk) require_value "$@"; sdk=$2; shift 2 ;;
        --native-only) native_only=1; shift ;;
        -h|--help)
            printf '%s\n' 'Usage: scripts/build.sh [--native-only] [--matlab ROOT] [--cc COMPILER] [--sdk SDK]'
            exit 0 ;;
        *) die "Unknown option: $1" ;;
    esac
done
require_arm64
if [[ -z $cc ]]; then
    cc=$(xcrun --find clang 2>/dev/null) || cc=/Library/Developer/CommandLineTools/usr/bin/clang
fi
if [[ -z $sdk ]]; then
    sdk=$(xcrun --show-sdk-path 2>/dev/null) || sdk=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
fi
command -v "$cc" >/dev/null || die 'Install Apple Command Line Tools, or specify --cc.'
[[ -d $sdk ]] || die 'Install a macOS SDK, or specify --sdk.'
if [[ $native_only == 0 ]]; then matlab=$(resolve_matlab "$matlab"); else matlab=; fi
mkdir -p "$BUILD"
common=("$cc" -O2 -Wall -Wextra -Werror -frounding-math -isysroot "$sdk")
"${common[@]}" -fblocks -dynamiclib "$ROOT/src/rounding_interpose.c" -o "$BUILD/libaccelerate_rounding.dylib"
for name in witness stress benchmark; do
    "${common[@]}" -framework Accelerate "$ROOT/tests/$name.c" -o "$BUILD/$name"
done
"${common[@]}" -fblocks -framework Accelerate "$ROOT/tests/scope.c" -o "$BUILD/scope"
"${common[@]}" "$ROOT/tests/policy.c" -o "$BUILD/policy"
if [[ -n $matlab ]]; then
    lib="$matlab/bin/maca64"
    "${common[@]}" -fPIC -bundle "-I$matlab/extern/include" "-L$lib" "-Wl,-rpath,$lib" -lmex -lmx \
        "$ROOT/src/rounding_patch_status.c" -o "$BUILD/rounding_patch_status.mexmaca64"
fi
# Plain data, never sourced as shell code. An empty root denotes native-only.
printf '%s\n' "$matlab" > "$BUILD/matlab-root.txt"
printf 'compiler=%s\nsdk=%s\npatch_abi=2\n' "$cc" "$sdk" > "$BUILD/build-info.txt"
printf 'Built in %s\n' "$BUILD"
