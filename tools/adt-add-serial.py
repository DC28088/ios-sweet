#!/usr/bin/env python3
"""Add root serial-number / mlb-serial-number placeholders to an existing ADT.

    tools/adt-add-serial.py IN.adt OUT.adt

MobileGestalt's SerialNumber (Settings > General > About) is IODeviceTree:/serial-number; on a
real D22AP iBoot fills it, and mlb-serial-number, from syscfg (SrNm, MLB#: 0x20 bytes each). Here
both are 32-byte values holding the placeholder "SWEET-SERIAL-FROM-ABL", which the shim
(shim/loader.c adt_fill_serial) overwrites at boot with ABL's androidboot.serialno. The phone's
serial is therefore never in a build artifact or in git.
"""
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import adt  # noqa: E402

PLACEHOLDER = b'SWEET-SERIAL-FROM-ABL'
KEYS = ('serial-number', 'mlb-serial-number')


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    src, dst = sys.argv[1:]
    blob = open(src, 'rb').read()
    root, end = adt.parse(blob)
    assert end == len(blob), 'trailing bytes'
    have = {n for n, _, _ in root.props}
    assert not have & set(KEYS), 'already has %s' % (have & set(KEYS))
    value = PLACEHOLDER.ljust(32, b'\0')
    added = [(k, 0, value) for k in KEYS]
    root.props[1:1] = added          # after 'name'-ish first property; order is not significant
    out = adt.serialize(root)
    _, end2 = adt.parse(out)
    assert end2 == len(out)
    assert out.count(PLACEHOLDER) == len(KEYS)
    del root.props[1:1 + len(KEYS)]
    assert adt.serialize(root) == blob, 'input did not round-trip'
    open(dst, 'wb').write(out)
    print('%s: %u bytes (was %u), %s placeholders, sha256 %s' % (dst, len(out), len(blob), '+'.join(KEYS),
          hashlib.sha256(out).hexdigest()))


if __name__ == '__main__':
    main()
