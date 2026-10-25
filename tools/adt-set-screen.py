#!/usr/bin/env python3
"""Set the presented screen of an existing ADT: /arm-io/disp0 canvas and /product artwork-device-subtype.

    tools/adt-set-screen.py IN.adt OUT.adt mini|x

The kernel's sweetfb_mode must match (SweetFramebuffer.cpp sweet_fb_logical_size()):
  mini  iPhone 12/13 mini, 1080x2340 @3x, subtype 2340 (sweetfb_mode=2, the kernel default from 0163):
        the panel's full width pixel for pixel, 30 black rows above and below
  x     iPhone X, 1125x2436 @3x, subtype 2436 (sweetfb_mode=1): 23 px cut off left and right
"""
import hashlib
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import adt  # noqa: E402

SCREENS = {'mini': (1080, 2340, 2340), 'x': (1125, 2436, 2436)}


def u32(v):
    return struct.pack('<I', v)


def walk(n):
    yield n
    for c in n.children:
        yield from walk(c)


def setprop(node, key, value):
    for i, (k, f, v) in enumerate(node.props):
        if k == key:
            assert len(v) == len(value), (key, len(v))
            old = v
            node.props[i] = (k, f, value)
            return old
    raise SystemExit('%s has no %s' % (node.name, key))


def main():
    if len(sys.argv) != 4 or sys.argv[3] not in SCREENS:
        raise SystemExit(__doc__)
    src, dst, screen = sys.argv[1:]
    width, height, subtype = SCREENS[screen]
    blob = open(src, 'rb').read()
    root, end = adt.parse(blob)
    assert end == len(blob), 'trailing bytes'
    disp0 = [n for n in walk(root) if n.name == 'disp0']
    product = [c for c in root.children if c.name == 'product']
    assert len(disp0) == 1 and len(product) == 1
    for node, key, value in ((disp0[0], 'canvas-width', width), (disp0[0], 'canvas-height', height),
                             (product[0], 'artwork-device-subtype', subtype)):
        old = setprop(node, key, u32(value))
        print('%s %s: %u -> %u' % (node.name, key, struct.unpack('<I', old)[0], value))
    out = adt.serialize(root)
    assert len(out) == len(blob)
    assert adt.parse(out)[1] == len(out)
    open(dst, 'wb').write(out)
    print('%s: %u bytes, sha256 %s' % (dst, len(out), hashlib.sha256(out).hexdigest()))


if __name__ == '__main__':
    main()
