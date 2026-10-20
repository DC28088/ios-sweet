#!/usr/bin/env python3
"""Add a locally verified A618 GMU image to an existing boot ADT.

Firmware stays in the local boot image; this tool does not download it.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import zlib

import adt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('base_adt', type=Path)
    parser.add_argument('gmu_firmware', type=Path)
    parser.add_argument('output_adt', type=Path)
    parser.add_argument('--sha256', required=True,
                        help='Expected SHA-256 of the locally obtained firmware')
    args = parser.parse_args()
    firmware = args.gmu_firmware.read_bytes()
    digest = hashlib.sha256(firmware).hexdigest()
    if digest != args.sha256.lower():
        parser.error(f'GMU SHA-256 mismatch: got {digest}')
    if len(firmware) != 0x8000:
        parser.error('A618 legacy GMU image must be exactly 32768 bytes')
    sp, pc = struct.unpack_from('<II', firmware)
    if sp % 8 or not 0x40000 <= sp <= 0x44000 or not pc & 1 or pc & ~1 >= len(firmware):
        parser.error('GMU image has an invalid Cortex-M vector table')
    source = args.base_adt.read_bytes()
    root, end = adt.parse(source)
    if end != len(source):
        parser.error('Base ADT has trailing data')
    if any(child.name == 'sweet-adreno' for child in root.children):
        parser.error('Base ADT already contains sweet-adreno; use the original tree')
    node = adt.Node()
    node.props = [('name', 0, b'sweet-adreno\0'),
                  ('compatible', 0, b'sweet,adreno618\0'),
                  ('gmu-firmware', 0, firmware),
                  ('gmu-crc32', 0, struct.pack('<I', zlib.crc32(firmware))),
                  ('gmu-sha256', 0, bytes.fromhex(digest))]
    root.children.append(node)
    output = adt.serialize(root)
    # Exclusive create preserves the chosen baseline and previous images.
    with args.output_adt.open('xb') as stream:
        stream.write(output)
    print(f'Wrote {args.output_adt}: GMU {len(firmware)} bytes, SHA-256 {digest}')


if __name__ == '__main__':
    main()
