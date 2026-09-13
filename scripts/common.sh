#!/bin/bash
# Shared helpers for macOS's bundled Bash 3.2. Source from scripts/*.sh.
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
BUILD="$ROOT/build"

die() { printf 'Error: %s\n' "$*" >&2; exit 1; }
require_arm64() {
    [[ $(uname -s) == Darwin && $(uname -m) == arm64 ]] ||
        die 'Native macOS arm64 is required.'
}
require_value() {
    [[ $# -ge 2 && -n $2 ]] || die "$1 requires a value."
}

# Return a physical installation root, accepting either .app or bin/matlab.
resolve_matlab() {
    local value=${1:-${MATLAB_ROOT:-}} candidates=()
    if [[ -z $value ]]; then
        if [[ -d /Applications/MATLAB_R2026a.app ]]; then
            value=/Applications/MATLAB_R2026a.app
        else
            for value in /Applications/MATLAB_*.app; do
                [[ ! -d $value ]] || candidates+=("$value")
            done
            [[ ${#candidates[@]} == 1 ]] ||
                die 'Specify --matlab /path/to/MATLAB.app (or MATLAB_ROOT).'
            value=${candidates[0]}
        fi
    fi
    case $value in
        '~/'*) value="$HOME/${value:2}" ;;
    esac
    value=${value%/}
    [[ $value != */bin/matlab ]] || value=${value%/bin/matlab}
    [[ -f $value/bin/matlab ]] || die "MATLAB bin/matlab not found under $value"
    (cd -- "$value" && pwd -P)
}

# Quote paths as literal data in MATLAB's POSIX-shell launch rc.
shell_quote() {
    printf "'"
    printf '%s' "$1" | sed "s/'/'\\\\''/g"
    printf "'"
}
