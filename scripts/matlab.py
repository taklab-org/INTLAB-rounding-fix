#!/usr/bin/env python3
"""Launch MATLAB with this checkout's patch; leave global configuration intact."""
import argparse
import json
import os
from pathlib import Path
import platform
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument('--matlab', help='Override the configured MATLAB .app/root or bin/matlab')
    parser.add_argument('--check', action='store_true', help='Run an isolated MATLAB check and exit')
    options, args = parser.parse_known_args()
    if args[:1] == ['--']:
        args = args[1:]
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        parser.error('Native macOS arm64 is required.')
    build = ROOT / 'build'
    patch = build / 'libaccelerate_rounding.dylib'
    config_file = build / 'config.json'
    if not patch.is_file() or not config_file.is_file():
        parser.error('Run python3 scripts/build.py first.')
    if not (build / 'rounding_patch_status.mexmaca64').is_file():
        parser.error('MATLAB MEX is missing; build without --native-only first.')
    config = json.loads(config_file.read_text())
    value = options.matlab or os.environ.get('MATLAB_ROOT') or config.get('matlab_root')
    if not value:
        parser.error('A MATLAB build is required; rerun build.py without --native-only.')
    matlab = Path(value).expanduser().resolve()
    if not (matlab.name == 'matlab' and matlab.parent.name == 'bin'):
        matlab = matlab / 'bin/matlab'
    if not matlab.is_file():
        parser.error('The configured MATLAB executable is missing; rebuild with --matlab.')
    if options.check:
        if args:
            parser.error('--check does not accept additional MATLAB arguments.')
        args = ['-sd', str(ROOT / 'tests'), '-batch', 'rounding_patch_check();']
    else:
        if not args:
            args = ['-desktop']
        if '-sd' not in args:
            args = ['-sd', os.getcwd()] + args
    env = os.environ.copy()
    # Only function directories, without startup.m; preserve the user's path.
    extra = os.pathsep.join([str(ROOT / 'matlab'), str(build)])
    env['MATLABPATH'] = extra + (os.pathsep + env['MATLABPATH'] if env.get('MATLABPATH') else '')
    with tempfile.TemporaryDirectory(prefix='accelerate-rounding-launch-') as directory:
        # Set DYLD_* after entering the system shell, which can strip inherited
        # DYLD variables. The original release defaults are still sourced.
        rc = '. ' + shlex.quote(str(matlab.parent / '.matlab7rc.sh')) + '\n'
        rc += 'export BLAS_VERSION=libmwAF_BLAS_ilp64.dylib\n'
        rc += 'export DYLD_INSERT_LIBRARIES=' + shlex.quote(str(patch)) + '\n'
        (Path(directory) / '.matlab7rc.sh').write_text(rc)
        try:
            result = subprocess.call([str(matlab)] + args, cwd=directory, env=env)
        except KeyboardInterrupt:
            result = 130
    return result


if __name__ == '__main__':
    sys.exit(main())
