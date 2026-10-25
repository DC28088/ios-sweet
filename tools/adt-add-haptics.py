#!/usr/bin/env python3
"""Add /product/haptics to an existing ADT.

    tools/adt-add-haptics.py IN.adt OUT.adt

MobileGestalt's DeviceSupportsClosedLoopHaptics is this node's closed-loop. With it iOS plays
its system feedbacks and vibrations as waveforms on the CoreAudio HAL "Actuator" device; without
it iOS looks for a legacy vibration motor in IOKit, which an iPhone X does not have (HackiOS
xnuloader/mkidentity.py). The values are the D22AP tree's own: closed-loop 1,
supports-3rd-party-haptics 1. mkadt.py copies only /product's properties, not its child nodes.
"""
import hashlib
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import adt  # noqa: E402


def cstr(s):
    return s.encode() + b'\0'


def u32(v):
    return struct.pack('<I', v)


def walk(n):
    yield n
    for c in n.children:
        yield from walk(c)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    src, dst = sys.argv[1:]
    blob = open(src, 'rb').read()
    root, end = adt.parse(blob)
    assert end == len(blob), 'trailing bytes'
    product = [c for c in root.children if c.name == 'product']
    assert len(product) == 1
    product = product[0]
    assert not any(c.name == 'haptics' for c in product.children), 'already has haptics'
    top = max(struct.unpack('<I', n.prop('AAPL,phandle')[:4])[0] for n in walk(root) if n.prop('AAPL,phandle'))
    hap = adt.Node()
    hap.props = [('closed-loop', 0, u32(1)), ('name', 0, cstr('haptics')), ('AAPL,phandle', 0, u32(top + 1)),
                 ('supports-3rd-party-haptics', 0, u32(1))]
    product.children.append(hap)
    out = adt.serialize(root)
    check, end2 = adt.parse(out)
    assert end2 == len(out)
    del product.children[-1]
    assert adt.serialize(root) == blob, 'input did not round-trip'
    open(dst, 'wb').write(out)
    print('%s: %u bytes (was %u), haptics phandle %u, sha256 %s' % (dst, len(out), len(blob), top + 1,
          hashlib.sha256(out).hexdigest()))


if __name__ == '__main__':
    main()
