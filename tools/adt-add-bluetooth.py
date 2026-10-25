#!/usr/bin/env python3
"""Add /arm-io/bluetooth to an existing ADT, with the WCN3990's Bluetooth firmware.

    tools/adt-add-bluetooth.py IN.adt OUT.adt FIRMWARE_DIR

bluetoothd and BlueTool only start when a registry entry named "bluetooth" with
local-mac-address exists; they read transport-encoding (3 = H4BC, the stock iPhone X value)
and transport-speed from it (Bergamota docs/BT_IOS_TRANSPORT.md). The kernel driver
(bsd/dev/sweet_bt.c) reads its rampatch and NVM from fw-crbtfw21.tlv / fw-crnv21.bin here,
copied from sweet's own `bluetooth` partition (image/crbtfw21.tlv, image/crnv21.bin).
Qualcomm firmware: the output ADT stays local (shim/out), never in git.
No function-* properties: the kernel owns power and reset.
"""
import hashlib
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import adt  # noqa: E402

BT_MAC = bytes.fromhex('025357424c31')     # locally administered
FILES = ('crbtfw21.tlv', 'crnv21.bin')


def cstr(s):
    return s.encode() + b'\0'


def u32(v):
    return struct.pack('<I', v)


def walk(n):
    yield n
    for c in n.children:
        yield from walk(c)


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    src, dst, fwdir = sys.argv[1:]
    blob = open(src, 'rb').read()
    root, end = adt.parse(blob)
    assert end == len(blob), 'trailing bytes'
    armio = [c for c in root.children if c.name == 'arm-io']
    assert len(armio) == 1
    armio = armio[0]
    assert not any(c.name == 'bluetooth' for c in armio.children), 'already has bluetooth'
    top = max(struct.unpack('<I', n.prop('AAPL,phandle')[:4])[0] for n in walk(root) if n.prop('AAPL,phandle'))
    bt = adt.Node()
    bt.props = [('name', 0, cstr('bluetooth')), ('AAPL,phandle', 0, u32(top + 1)),
                ('device_type', 0, cstr('bluetooth')), ('compatible', 0, cstr('bluetooth,wcn3990')),
                ('local-mac-address', 0, BT_MAC), ('transport-encoding', 0, u32(3)),
                ('transport-speed', 0, u32(3000000)), ('vendor-id', 0, u32(0x05ac)),
                ('product-id', 0, u32(0x12a0))]
    for f in FILES:
        data = open(os.path.join(fwdir, f), 'rb').read()
        bt.props.append(('fw-' + f, 0, data))
        print('%s: %u bytes sha256 %s' % (f, len(data), hashlib.sha256(data).hexdigest()))
    armio.children.append(bt)
    out = adt.serialize(root)
    check, end2 = adt.parse(out)
    assert end2 == len(out)
    del armio.children[-1]
    assert adt.serialize(root) == blob, 'input did not round-trip'
    open(dst, 'wb').write(out)
    print('%s: %u bytes (was %u), bluetooth phandle %u, sha256 %s' % (dst, len(out), len(blob), top + 1,
          hashlib.sha256(out).hexdigest()))


if __name__ == '__main__':
    main()
