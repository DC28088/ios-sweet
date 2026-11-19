#!/usr/bin/env python3
"""Set /product baseband-chipset of an existing ADT (same-length string, e.g. ice19 -> mav20).

    tools/adt-set-bbchip.py IN.adt OUT.adt CHIPSET

iOS 26 N104 (iPhone 11) says 'ice19' (Intel), so CommCenter loads the Intel (IBI) drivers; sweet's modem speaks
Qualcomm QMI through the kernel bridge (sweetbb=1). MobileGestalt's BasebandChipset follows this property (and its
cache's CacheExtra copy, which must be changed too).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import adt  # noqa: E402


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    src, dst, chip = sys.argv[1:]
    blob = open(src, 'rb').read()
    root, end = adt.parse(blob)
    assert end == len(blob), 'trailing bytes'
    product = [c for c in root.children if c.name == 'product']
    assert len(product) == 1
    node = product[0]
    for i, (k, f, v) in enumerate(node.props):
        if k == 'baseband-chipset':
            new = chip.encode() + b'\0'
            if len(new) != len(v):
                raise SystemExit('length %u != %u' % (len(new), len(v)))
            node.props[i] = (k, f, new)
            print('product baseband-chipset: %r -> %r' % (v, new))
            break
    else:
        raise SystemExit('/product has no baseband-chipset')
    out = adt.serialize(root)
    assert len(out) == len(blob) and adt.parse(out)[1] == len(out)
    open(dst, 'wb').write(out)


if __name__ == '__main__':
    main()
