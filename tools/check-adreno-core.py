#!/usr/bin/env python3
"""Compile and exercise the portable driver from its authoritative XNU patches."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='sweet-adreno-check-') as temporary:
    work = Path(temporary)
    subprocess.run(['git', 'init', '-q', str(work)], check=True)
    for name in ('0088-sweet-adreno-core.patch', '0089-sweet-adreno-gmu-clocks.patch'):
        subprocess.run(['git', '-C', str(work), 'apply',
                        '--include=bsd/dev/sweet_adreno.c',
                        '--include=bsd/dev/sweet_adreno.h',
                        '--include=bsd/dev/sweet_adreno_clock.c',
                        str(root / 'xnu/patches' / name)], check=True)
    command = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c11', '-Wall', '-Wextra', '-Werror', '-DARM64_BOARD_CONFIG_SWEET',
        '-I', str(work / 'bsd/dev'), str(root / 'tests/adreno-core-check.c'),
        str(work / 'bsd/dev/sweet_adreno_clock.c'), '-o', str(work / 'check')]
    subprocess.run(command, check=True)
    subprocess.run([str(work / 'check')], check=True)
