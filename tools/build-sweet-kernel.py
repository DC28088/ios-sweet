#!/usr/bin/env python3
"""Compile the sweet Darwin22 kernel without the prelink step.

`unixnu build kernel` always prelinks the result against the original
kernelcache. The first Darwin22 milestone is only "does the adapted source
queue compile and link", and the prelink roots/slide for 20H392 are not
audited yet. This wrapper calls the same UniXNU kernel build (same defconfig
environment, archive builders and bootstrap pass) and replaces only the
prelink call with an explicit no-op.

This produces a kernel, not a bootable payload and not a hardware result.
"""

import argparse
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--unixnu', required=True, type=Path,
                        help='pinned UniXNU checkout (provides scripts/unixnu)')
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--device', required=True, type=Path)
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()

    package = args.unixnu / 'scripts'
    if not (package / 'unixnu/cli.py').is_file():
        raise SystemExit(f'{package} does not contain the unixnu package')
    sys.path.insert(0, str(package))
    from unixnu import cli, kernel

    def no_prelink(ctx):
        print('kernel: prelink intentionally skipped for the first Darwin22 compile')

    kernel.prelink = no_prelink
    return cli.main([
        'build', 'kernel',
        '-D', str(args.device),
        '--out', str(args.out),
        '--jobs', str(args.jobs),
    ])


if __name__ == '__main__':
    sys.exit(main())
