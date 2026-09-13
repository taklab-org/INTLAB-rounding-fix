#!/usr/bin/env python3
"""Build the patch and diagnostics locally; no installation or downloads."""
import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def matlab_root(value=None):
    value = value or os.environ.get('MATLAB_ROOT')
    if value:
        root = Path(value).expanduser().resolve()
        if root.name == 'matlab' and root.parent.name == 'bin':
            root = root.parent.parent
    else:
        preferred = Path('/Applications/MATLAB_R2026a.app')
        candidates = sorted(Path('/Applications').glob('MATLAB_*.app'))
        if preferred.is_dir():
            root = preferred
        elif len(candidates) == 1:
            root = candidates[0]
        else:
            raise ValueError('Specify --matlab /path/to/MATLAB.app (or MATLAB_ROOT).')
    if not (root / 'bin/matlab').is_file():
        raise ValueError('MATLAB bin/matlab not found under ' + str(root))
    return root


def developer_tool(option, fallback):
    result = subprocess.run(['xcrun'] + option, capture_output=True, text=True)
    candidate = Path(result.stdout.strip()) if result.returncode == 0 else fallback
    if not candidate.exists():
        raise ValueError('Install Apple Command Line Tools, or specify --cc and --sdk.')
    return str(candidate)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--matlab', help='MATLAB .app/root or bin/matlab')
    parser.add_argument('--native-only', action='store_true', help='Do not build the MATLAB MEX')
    parser.add_argument('--cc', help='C compiler path')
    parser.add_argument('--sdk', help='macOS SDK directory')
    args = parser.parse_args()
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        parser.error('This project currently supports native macOS arm64 only.')
    try:
        cc = args.cc or developer_tool(['--find', 'clang'], Path('/Library/Developer/CommandLineTools/usr/bin/clang'))
        sdk = args.sdk or developer_tool(['--show-sdk-path'], Path('/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk'))
        matlab = None if args.native_only else matlab_root(args.matlab)
    except ValueError as error:
        parser.error(str(error))
    build = ROOT / 'build'
    build.mkdir(exist_ok=True)
    common = [cc, '-O2', '-Wall', '-Wextra', '-Werror', '-frounding-math', '-isysroot', sdk]
    commands = [common + ['-fblocks', '-dynamiclib', str(ROOT / 'src/rounding_interpose.c'),
                           '-o', str(build / 'libaccelerate_rounding.dylib')]]
    for name in ('witness', 'stress', 'benchmark'):
        commands.append(common + ['-framework', 'Accelerate', str(ROOT / 'tests' / (name + '.c')),
                                  '-o', str(build / name)])
    if matlab:
        lib = matlab / 'bin/maca64'
        commands.append(common + ['-fPIC', '-bundle', '-I' + str(matlab / 'extern/include'),
            '-L' + str(lib), '-Wl,-rpath,' + str(lib), '-lmex', '-lmx',
            str(ROOT / 'src/rounding_patch_status.c'), '-o', str(build / 'rounding_patch_status.mexmaca64')])
    for command in commands:
        subprocess.run(command, check=True)
    config = {'matlab_root': str(matlab) if matlab else None, 'compiler': cc,
              'sdk': sdk, 'platform': platform.platform(), 'patch_abi': 1}
    (build / 'config.json').write_text(json.dumps(config, indent=2) + '\n')
    print('Built in', build)


if __name__ == '__main__':
    main()
