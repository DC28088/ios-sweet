#!/usr/bin/env python3
"""Add a /baseband node to an existing ADT (the iOS cellular stack's switch).

    tools/adt-add-baseband.py IN.adt OUT.adt [REAL_D22.adt]

launchd loads com.apple.CommCenter (instead of the -noncellular variant) only when the device tree has
/baseband (_LimitLoadToDeviceTree), and CommCenter/ABM read its identity properties. On a real D22AP the
node also carries function-* GPIO/PMU references (bb_on, bb_rst, pcie_port_control, ...) for Apple's PCIe
baseband driver; this SoC's modem is booted by wland and reached through the kernel bridge (xnu 0184,
sweetbb=1), so only the identity properties are copied: name, device_type, compatible "baseband,n41",
class, imeisv, config. device-imei stays 32 zero bytes and region-sku is 0x40 zero bytes (iBoot fills both
from syscfg on Apple hardware; the template string is not copied). With REAL_D22.adt the values are taken
from it and checked against the built-in ones.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import adt  # noqa: E402

PROPS = [
    ('name', b'baseband\0'),
    ('device_type', b'baseband\0'),
    ('compatible', b'baseband,n41\0'),
    ('class', bytes.fromhex('03000000')),
    ('imeisv', bytes.fromhex('39000000')),
    ('config', bytes.fromhex('fa000000f4010000444800' '00e8030000640000006400000083000000250000001400000064000000')),
    ('device-imei', bytes(32)),
    ('region-sku', bytes(0x40)),
]


def node_name(n):
    return next((v for k, _, v in n.props if k == 'name'), b'').rstrip(b'\0')


def main():
    if len(sys.argv) not in (3, 4):
        raise SystemExit(__doc__)
    src, dst = sys.argv[1:3]
    if len(sys.argv) == 4:
        real, _ = adt.parse(open(sys.argv[3], 'rb').read())
        bb = next(c for c in real.children if node_name(c) == b'baseband')
        have = {k: v for k, _, v in bb.props}
        for k, v in PROPS:
            if k in ('device-imei', 'region-sku'):
                continue
            assert have[k] == v, (k, have[k], v)
    blob = open(src, 'rb').read()
    root, end = adt.parse(blob)
    assert end == len(blob), 'trailing bytes'
    assert not any(node_name(c) == b'baseband' for c in root.children), 'already has /baseband'
    node = adt.Node()
    node.props = [(k, 0, v) for k, v in PROPS]
    root.children.append(node)
    out = adt.serialize(root)
    check, end2 = adt.parse(out)
    assert end2 == len(out)
    assert [k for k, _, _ in check.children[-1].props] == [k for k, _ in PROPS]
    root.children.pop()
    assert adt.serialize(root) == blob, 'input did not round-trip'
    open(dst, 'wb').write(out)
    print('%s: /baseband (%d properties) added, %d -> %d bytes' % (dst, len(PROPS), len(blob), len(out)))


if __name__ == '__main__':
    main()
