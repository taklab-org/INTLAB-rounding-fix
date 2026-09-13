#!/bin/bash
# Opt-in MATLAB launcher. Leave global configuration and normal startup intact.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/common.sh"
matlab= check=0 args=()
while [[ $# -gt 0 ]]; do
    case $1 in
        --matlab) require_value "$@"; matlab=$2; shift 2 ;;
        --check) check=1; shift ;;
        --help)
            printf '%s\n' 'Usage: scripts/matlab.sh [--matlab ROOT] [--check | MATLAB arguments]'
            exit 0 ;;
        --) shift; args+=("$@"); break ;;
        *) args+=("$1"); shift ;;
    esac
done
require_arm64
patch="$BUILD/libaccelerate_rounding.dylib"
[[ -f $patch && -f $BUILD/matlab-root.txt ]] || die 'Run scripts/build.sh first.'
[[ -f $BUILD/rounding_patch_status.mexmaca64 ]] || die 'MATLAB MEX is missing; build without --native-only first.'
if [[ -z $matlab ]]; then
    matlab=${MATLAB_ROOT:-}
    if [[ -z $matlab ]]; then IFS= read -r matlab < "$BUILD/matlab-root.txt"; fi
fi
[[ -n $matlab ]] || die 'A MATLAB build is required; rerun build.sh without --native-only.'
matlab=$(resolve_matlab "$matlab")/bin/matlab
if [[ $check == 1 ]]; then
    [[ ${#args[@]} == 0 ]] || die '--check does not accept additional MATLAB arguments.'
    args=(-sd "$ROOT/tests" -batch 'rounding_patch_check();')
else
    if [[ ${#args[@]} == 0 ]]; then args=(-desktop); fi
    has_sd=0
    for arg in "${args[@]}"; do [[ $arg != -sd ]] || has_sd=1; done
    if [[ $has_sd == 0 ]]; then args=(-sd "$PWD" "${args[@]}"); fi
fi
export MATLABPATH="$ROOT/matlab:$BUILD${MATLABPATH:+:$MATLABPATH}"
launch_dir=$(mktemp -d "${TMPDIR:-/tmp}/accelerate-rounding-launch.XXXXXX")
trap 'rm -rf -- "$launch_dir"' EXIT
# Set DYLD_* inside MATLAB's launch shell: macOS may strip inherited values
# when starting a protected system executable. Keep original release defaults.
{
    printf '. '; shell_quote "$(dirname -- "$matlab")/.matlab7rc.sh"; printf '\n'
    printf 'export BLAS_VERSION=libmwAF_BLAS_ilp64.dylib\n'
    printf 'export DYLD_INSERT_LIBRARIES='; shell_quote "$patch"; printf '\n'
} > "$launch_dir/.matlab7rc.sh"
cd -- "$launch_dir"
"$matlab" "${args[@]}"
