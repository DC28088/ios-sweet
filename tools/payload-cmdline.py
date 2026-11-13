#!/usr/bin/env python3
"""Print or replace the boot-args of an existing SWEETXNU payload (tools/mkpayload.py layout).

  payload-cmdline.py PAYLOAD                       print the cmdline
  payload-cmdline.py PAYLOAD OUT --set "ARGS"      write a copy with ARGS as the whole cmdline
  payload-cmdline.py PAYLOAD OUT --edit k=v ...    copy with each k=v (or -flag) replaced/added, k=- removes k

Only the 608-byte cmdline field at 0x48 changes; kernel, ADT and segment table stay byte-identical,
so a payload built by another mkpayload (e.g. one that adds ADT nodes) keeps everything else.
"""
import sys

MAGIC = b'SWEETXNU'
CMDLINE, LEN = 0x48, 608


def get(blob):
    assert blob[:8] == MAGIC, 'not a SWEETXNU payload'
    return blob[CMDLINE:CMDLINE + LEN].split(b'\0')[0].decode()


def put(blob, cmd):
    b = cmd.encode()
    if len(b) > LEN - 1:
        raise SystemExit(f'cmdline too long: {len(b)} > {LEN - 1}')
    out = bytearray(blob)
    out[CMDLINE:CMDLINE + LEN] = b + bytes(LEN - len(b))
    return bytes(out)


def edit(cmd, changes):
    args = cmd.split()
    for ch in changes:
        key = ch.split('=', 1)[0]
        args = [a for a in args if a.split('=', 1)[0] != key]
        if not ch.endswith('=-'):
            args.append(ch)
    return ' '.join(args)


if __name__ == '__main__':
    blob = open(sys.argv[1], 'rb').read()
    cur = get(blob)
    if len(sys.argv) == 2:
        print(cur)
        sys.exit(0)
    out, mode, rest = sys.argv[2], sys.argv[3], sys.argv[4:]
    new = ' '.join(rest) if mode == '--set' else edit(cur, rest) if mode == '--edit' else sys.exit('--set or --edit')
    open(out, 'wb').write(put(blob, new))
    print(new)
