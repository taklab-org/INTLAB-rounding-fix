#!/usr/bin/env python3
"""Compare unpatched and patched native BLAS; retain raw logs under build/."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'


def main():
    library = BUILD / 'libaccelerate_rounding.dylib'
    if not library.is_file():
        sys.exit('Run scripts/build.py first (or build.py --native-only).')
    results = []
    for patched in (False, True):
        for name in ('witness', 'stress'):
            env = os.environ.copy()
            # Ensure the control process has no inherited interposer.
            env.pop('DYLD_INSERT_LIBRARIES', None)
            env.pop('VECLIB_MAXIMUM_THREADS', None)
            env['ACCELERATE_ROUNDING_AUDIT'] = '1'
            if patched:
                env['DYLD_INSERT_LIBRARIES'] = str(library)
            result = subprocess.run([str(BUILD / name)], env=env, capture_output=True, text=True)
            label = name + ('-patched' if patched else '-baseline')
            (BUILD / (label + '.log')).write_text(result.stdout + result.stderr)
            if result.returncode not in (0, 1):
                sys.exit('Test did not complete: ' + label)
            sentinel = 'WITNESS_COMPLETE' if name == 'witness' else 'STRESS_COMPLETE'
            if sentinel not in result.stdout:
                sys.exit('Missing completion marker: ' + label)
            if patched:
                if result.returncode:
                    sys.exit('Patched enclosure test failed: ' + label)
                match = re.search(r'foreign_thread_callbacks=(\d+)', result.stderr)
                if not match or int(match.group(1)) == 0:
                    sys.exit('No BLAS worker interception observed; parallel coverage unverified: ' + label)
                peak = re.search(r'peak_overlapping_callbacks=(\d+)', result.stderr)
                if not peak or int(peak.group(1)) < 2:
                    sys.exit('No overlapping BLAS callbacks observed; parallel coverage unverified: ' + label)
            results.append({'test': name, 'patched': patched, 'exit_code': result.returncode})
            print(label, 'PASS' if result.returncode == 0 else 'rounding failure reproduced')
    (BUILD / 'native-results.json').write_text(json.dumps(results, indent=2) + '\n')
    print('NATIVE_TESTS_COMPLETE')


if __name__ == '__main__':
    main()
