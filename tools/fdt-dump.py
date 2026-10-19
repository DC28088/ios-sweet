#!/usr/bin/env python3
"""Minimal flattened-device-tree reader for local hardware evidence.

  fdt-dump.py DTB [--grep TEXT] [--path PATH]

Prints node paths and properties as hex/ASCII. No libfdt needed (the build
host does not ship dtc). With --grep, only nodes whose path or a property
name/value (as text) contains TEXT are printed; --path prints one subtree.
"""
import struct
import sys


def u32(b, o):
    return struct.unpack_from('>I', b, o)[0]


def cells(b):
    if len(b) % 4:
        return ' '.join('%02x' % x for x in b)
    v = struct.unpack('>%dI' % (len(b) // 4), b)
    if len(v) == 1:
        return '%d (0x%x)' % (v[0], v[0])
    return ' '.join('<%d>' % x for x in v)


def parse(blob):
    magic, total, off_struct, off_strings, off_rsv, version, last_comp, boot_cpu, size_strings, size_struct = \
        struct.unpack_from('>10I', blob, 0)
    if magic != 0xd00dfeed:
        raise SystemExit('not a DTB (magic %#x)' % magic)
    strings = blob[off_strings:off_strings + size_strings]
    pos = off_struct
    path = []
    nodes = []
    node = None
    while True:
        tok = u32(blob, pos)
        pos += 4
        if tok == 1:  # FDT_BEGIN_NODE
            end = blob.index(b'\0', pos)
            name = blob[pos:end].decode('utf-8', 'replace')
            pos = (end + 4) & ~3
            path.append(name)
            node = {'path': '/' + '/'.join(p for p in path if p), 'props': []}
            nodes.append(node)
        elif tok == 2:  # FDT_END_NODE
            path.pop()
            node = None
        elif tok == 3:  # FDT_PROP
            ln = u32(blob, pos)
            nameoff = u32(blob, pos + 4)
            pos += 8
            val = blob[pos:pos + ln]
            pos = (pos + ln + 3) & ~3
            end = strings.index(b'\0', nameoff)
            name = strings[nameoff:end].decode()
            if node is not None:
                node['props'].append((name, val))
        elif tok == 4:  # FDT_NOP
            pass
        elif tok == 9:  # FDT_END
            break
        else:
            raise SystemExit('bad token %#x at %#x' % (tok, pos - 4))
    return nodes


def render(nodes, grep, want):
    for n in nodes:
        text = n['path'] + ' ' + ' '.join(
            p + '=' + v.decode('utf-8', 'replace') for p, v in n['props'])
        if want and not (n['path'] == want or n['path'].startswith(want + '/')):
            continue
        if grep and grep.lower() not in text.lower():
            continue
        print(n['path'])
        for name, val in n['props']:
            printable = ''.join(chr(c) if 32 <= c < 127 else '.' for c in val)
            if all(32 <= c < 127 or c == 0 for c in val) and val:
                shown = '"%s"' % printable.rstrip('.')
            else:
                shown = cells(val)
            print('    %-32s %s' % (name, shown[:400]))


if __name__ == '__main__':
    args = sys.argv[1:]
    if not args:
        raise SystemExit(__doc__)
    grep = None
    want = None
    if '--grep' in args:
        i = args.index('--grep')
        grep = args[i + 1]
        del args[i:i + 2]
    if '--path' in args:
        i = args.index('--path')
        want = args[i + 1]
        del args[i:i + 2]
    render(parse(open(args[0], 'rb').read()), grep, want)
