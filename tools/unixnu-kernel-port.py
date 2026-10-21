#!/usr/bin/env python3
"""Apply the pinned UniXNU XNU patch queue to an Apple XNU source tree.

The queue is the sorted patches/xnu directory of the pinned UniXNU revision.
Patches whose upstream context does not exist in the selected Apple source are
replaced by a same-named patch in the device kernel-port directory. Replacements
are applied instead of the upstream patch, never in addition to it.

This tool only applies or checks text patches. A clean application is not a
compile, a link or a hardware result.

Usage:
  tools/unixnu-kernel-port.py --source DIR --patches DIR --replacements DIR
                              [--check] [--keep-going] [--report PATH]
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path


def apply_patch(source, patch, check):
    command = ['git', '-C', str(source), 'apply', '--whitespace=nowarn']
    if check:
        command.append('--check')
    command.append(str(patch.resolve()))
    result = subprocess.run(command, capture_output=True, text=True)
    return result.returncode == 0, (result.stderr or result.stdout).strip()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--patches', required=True, type=Path)
    parser.add_argument('--replacements', required=True, type=Path)
    parser.add_argument('--check', action='store_true',
                        help='only test application; do not modify the source')
    parser.add_argument('--keep-going', action='store_true',
                        help='continue after a rejection instead of stopping')
    parser.add_argument('--report', type=Path,
                        help='write a JSON report of every queue entry')
    args = parser.parse_args()

    if not (args.source / 'osfmk').is_dir():
        raise SystemExit(f'{args.source} does not look like an XNU source tree')
    patches = sorted(args.patches.glob('*.patch'))
    if not patches:
        raise SystemExit(f'no patches in {args.patches}')

    skips = {}
    skip_file = args.replacements / 'skip.json'
    if skip_file.is_file():
        skips = json.loads(skip_file.read_text()).get('patches', {})

    results = []
    applied = 0
    rejected = 0
    skipped = 0
    for patch in patches:
        if patch.name in skips:
            skipped += 1
            results.append({
                'patch': patch.name,
                'origin': 'skipped',
                'applies': None,
                'diagnostic': skips[patch.name],
            })
            print(f'skip {patch.name}')
            continue
        replacement = args.replacements / patch.name
        used = replacement if replacement.is_file() else patch
        ok, diagnostic = apply_patch(args.source, used, args.check)
        origin = 'replacement' if replacement.is_file() else 'upstream'
        results.append({
            'patch': patch.name,
            'origin': origin,
            'applies': ok,
            'diagnostic': '' if ok else diagnostic,
        })
        if ok:
            applied += 1
            print(f'ok   {patch.name} ({origin})')
        else:
            rejected += 1
            print(f'FAIL {patch.name} ({origin})')
            print(diagnostic)
            if not args.keep_going:
                break

    report = {
        'source': str(args.source),
        'patches': str(args.patches),
        'replacements': str(args.replacements),
        'mode': 'check' if args.check else 'apply',
        'queue_total': len(patches),
        'checked': len(results),
        'applied': applied,
        'rejected': rejected,
        'skipped': skipped,
        'complete': len(results) == len(patches) and rejected == 0,
        'results': results,
    }
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(f'{applied} applied, {rejected} rejected, {skipped} skipped, '
          f'{len(results)}/{len(patches)} queue entries checked')
    return 0 if rejected == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
