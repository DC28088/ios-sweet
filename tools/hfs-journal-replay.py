#!/usr/bin/env python3
"""Replay a journaled HFS+ volume's pending transactions into a qcow2 overlay, as XNU does at mount.

    tools/hfs-journal-replay.py SOURCE OVERLAY.qcow2 [--dry-run]

SOURCE is the root-install adb-read-plugin.py (its pread reads the phone's userdata through the
same 1 MiB cache nbdkit uses); the overlay is written with qemu-io and must be a fresh overlay of
exactly that source. Why: the
phone's iOS root is left mid-journal whenever iOS is stopped by a forced reboot (0163: 4.3 MB
pending); Linux's fsck.hfsplus (hfsplus-tools 540.1) does not replay journals, so it reads stale
metadata (it then crashed rebuilding the catalog), and a write made over an unreplayed journal is
overwritten by iOS's replay at the next mount.

Format and order from XNU bsd/vfs/vfs_journal.c (replay_journal): journal_header {magic, endian,
start, end, size, blhdr_size, checksum, jhdr_size, sequence_num}; from start to end, a
block_list_header {max_blocks, num_blocks, bytes_used, checksum, flags} and binfo[num_blocks]
{bnum, bsize, next/cksum} (binfo[0] is the header's own), then the blocks' data in order; the
journal wraps from size back to jhdr_size; bnum is in jhdr_size units; bnum -1 is a killed block.
Header checksums use calc_checksum() over 32 bytes (block list) / 44 bytes (journal header) with
the checksum field zeroed. Replay stops at the first block list whose checksum fails. Afterwards
the header's start = end (the journal empty), its checksum recomputed.
"""
import os
import struct
import subprocess
import sys
import tempfile

MAGIC = 0x4a4e4c78
ENDIAN = 0x12345678


def checksum(data):
    c = 0
    for b in data:
        c = ((c << 8) ^ (c + b)) & 0xffffffff
    return (~c) & 0xffffffff


def read(src, offset, length):
    """SOURCE is an nbdkit "sh" plugin script (the root-install adb-read-plugin.py: pread COUNT OFFSET)."""
    data = subprocess.check_output([src, 'pread', 'root', str(length), str(offset)], timeout=600)
    assert len(data) == length, (offset, length, len(data))
    return data


def main():
    if len(sys.argv) not in (3, 4):
        raise SystemExit(__doc__)
    src, overlay = sys.argv[1:3]
    dry = len(sys.argv) == 4 and sys.argv[3] == '--dry-run'
    vh = read(src, 0, 4096)[1024:1536]
    sig, version, attributes, _, jib = struct.unpack('>HHIII', vh[:16])
    blocksize = struct.unpack('>I', vh[40:44])[0]
    assert sig in (0x482b, 0x4858) and attributes & 0x2000, 'not a journaled HFS+ volume'
    info = read(src, jib * blocksize, 4096)
    jflags, = struct.unpack('>I', info[:4])
    joff, jsize = struct.unpack('>QQ', info[36:52])
    assert jflags & 1, 'journal not in this volume'
    journal = read(src, joff, jsize)
    e = '>' if struct.unpack('>I', journal[4:8])[0] == ENDIAN else '<'
    magic, endian, start, end, size, blhdr_size, jcksum, jhdr_size, seq = struct.unpack(e + 'iiqqqiiiI', journal[:48])
    assert magic == MAGIC and endian == ENDIAN and size == jsize, (hex(magic), hex(endian), size, jsize)
    hdr0 = bytearray(journal[:44])
    hdr0[36:40] = b'\0\0\0\0'
    print('journal at %d, %d bytes, %s-endian: start %d end %d blhdr %d jhdr %d seq %u, header checksum %s' % (
        joff, size, 'big' if e == '>' else 'little', start, end, blhdr_size, jhdr_size, seq,
        'ok' if checksum(hdr0) == jcksum & 0xffffffff else 'BAD'))
    if start == end:
        print('journal empty: nothing to replay')
        return

    def jread(off, length):
        out = bytearray()
        while length:
            if off >= size:
                off = jhdr_size
            n = min(length, size - off)
            out += journal[off:off + n]
            off += n
            length -= n
        return bytes(out), (jhdr_size if off >= size else off)

    writes = {}
    order = []
    off = start
    txns = 0
    while off != end:
        hdr, _ = jread(off, blhdr_size)
        max_blocks, num_blocks, bytes_used, ck, flags = struct.unpack(e + 'HHiiI', hdr[:16])
        z = bytearray(hdr[:32])
        z[8:12] = b'\0\0\0\0'
        if checksum(z) != ck & 0xffffffff:
            print('transaction %d at %d: block list checksum BAD: replay stops here' % (txns, off))
            break
        if num_blocks < 1 or bytes_used < blhdr_size or num_blocks > max_blocks:
            print('transaction %d at %d: implausible header %d/%d/%d: replay stops here' % (
                txns, off, num_blocks, max_blocks, bytes_used))
            break
        _, data_off = jread(off, blhdr_size)
        for i in range(1, num_blocks):
            bnum, bsize = struct.unpack(e + 'qI', hdr[16 * i + 16:16 * i + 28])
            block, data_off = jread(data_off, bsize)
            if bnum == -1:
                continue
            target = bnum * jhdr_size
            if target not in writes:
                order.append(target)
            writes[target] = block
        consumed = (off + bytes_used) if off + bytes_used <= size else (off + bytes_used - size + jhdr_size)
        off = consumed if consumed < size else jhdr_size + (consumed - size)
        txns += 1
    print('%d transactions, %d distinct blocks, %d bytes' % (txns, len(writes), sum(len(v) for v in writes.values())))
    for t in sorted(order)[:6]:
        print('  block at %d (%d bytes)' % (t, len(writes[t])))
    new = bytearray(journal[:jhdr_size])
    struct.pack_into(e + 'q', new, 8, off)          # start = where replay ended (= end when complete)
    if off != end:
        print('replay incomplete (stopped at %d, end %d): the header gets start = end = %d' % (off, end, off))
        struct.pack_into(e + 'q', new, 16, off)
    h = bytearray(new[:44])
    h[36:40] = b'\0\0\0\0'
    struct.pack_into(e + 'i', new, 36, struct.unpack('>i', struct.pack('>I', checksum(h)))[0])
    if dry:
        print('dry run: nothing written')
        return
    with tempfile.TemporaryDirectory() as d:
        cmds = []
        for n, t in enumerate(order):
            p = os.path.join(d, '%d.bin' % n)
            open(p, 'wb').write(writes[t])
            cmds += ['-c', 'write -s %s %d %d' % (p, t, len(writes[t]))]
        p = os.path.join(d, 'jhdr.bin')
        open(p, 'wb').write(bytes(new))
        cmds += ['-c', 'write -s %s %d %d' % (p, joff, len(new))]
        for i in range(0, len(cmds), 400):
            subprocess.run(['qemu-io', '-f', 'qcow2'] + cmds[i:i + 400] + [overlay], check=True,
                           stdout=subprocess.DEVNULL)
    print('replayed into %s; journal header start = end = %d' % (overlay, off))


if __name__ == '__main__':
    main()
