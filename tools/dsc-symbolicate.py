#!/usr/bin/env python3
"""Name the "??? (Image + offset)" frames of an iOS spindump with the shared cache's local symbols.

    tools/dsc-symbolicate.py CACHE-DIR SPINDUMP.txt [MACHO...] > SPINDUMP.sym.txt

CACHE-DIR holds dyld_shared_cache_arm64 (the main file: its image table) and
dyld_shared_cache_arm64.symbols (the local symbols the phone's copy no longer carries), both from the
phone (/private/preboot/Cryptexes/OS/System/Library/Caches/com.apple.dyld/; research/dsc, git-ignored:
Apple's files). Reads the formats as dyld-1042 (iOS 16) lays them out: dyld_cache_header's image table
(imagesOffset/imagesCount at 0x1c0), the .symbols file's dyld_cache_local_symbols_info at its header's
localSymbolsOffset, 64-bit entries (dylibOffset from the shared region start) and nlist_64 records.
A frame becomes "Image`symbol + delta"; frames of images without symbols stay as they were.
MACHO files (our own binaries, e.g. SweetMetal2 and libvulkan_freedreno.dylib) name the frames spindump
prints as "??? (<UUID> + offset)" when their LC_UUID matches; a kernel (MH_EXECUTE named *kernel*, e.g.
xnu/out-0157-range/kernel.development.sweet) names "??? (__kernel__ + offset)" from its __TEXT address.
"""
import bisect
import re
import struct
import sys


def read_images(main):
    b = open(main, 'rb').read()
    assert b[:4] == b'dyld', 'not a dyld shared cache'
    shared_start = struct.unpack_from('<Q', b, 224)[0]
    off, count = struct.unpack_from('<II', b, 0x1c0)
    if count == 0:
        off, count = struct.unpack_from('<II', b, 24)
    images = {}
    for i in range(count):
        addr, _, _, path_off = struct.unpack_from('<QQQI', b, off + 32 * i)
        path = b[path_off:b.index(b'\0', path_off)].decode()
        name = path.rsplit('/', 1)[-1]
        # spindump names frames by basename; where two images share one (IOKit.framework and
        # IOKit.platform), the public framework or library is meant
        if name not in images or path.startswith(('/System/Library/Frameworks/', '/usr/lib/')):
            images[name] = (addr, path)
    return shared_start, images


def read_symbols(symfile, shared_start, wanted):
    """{image name: (sorted addresses, names)} for the images in `wanted` ({name: base address})."""
    f = open(symfile, 'rb')
    head = f.read(512)
    lso, lss = struct.unpack_from('<QQ', head, 72)
    f.seek(lso)
    info = f.read(24)
    nl_off, nl_count, str_off, str_size, ent_off, ent_count = struct.unpack('<6I', info)
    f.seek(lso + ent_off)
    entries = f.read(16 * ent_count)
    by_offset = {}
    for i in range(ent_count):
        dylib_off, start, count = struct.unpack_from('<QII', entries, 16 * i)
        by_offset[dylib_off] = (start, count)
    f.seek(lso + str_off)
    strings = f.read(str_size)
    out = {}
    for name, base in wanted.items():
        e = by_offset.get(base - shared_start)
        if not e:
            continue
        start, count = e
        f.seek(lso + nl_off + 16 * start)
        raw = f.read(16 * count)
        syms = []
        for k in range(count):
            strx, ntype, sect, desc, value = struct.unpack_from('<IBBHQ', raw, 16 * k)
            if ntype & 0xe0 or sect == 0 or value == 0:      # stabs and undefined/absolute out
                continue
            syms.append((value, strings[strx:strings.index(b'\0', strx)].decode(errors='replace')))
        syms.sort()
        out[name] = ([a for a, _ in syms], [n for _, n in syms])
    return out


def read_macho(path):
    """(UUID text, image name, sorted addresses, names) of a thin arm64 Mach-O's defined symbols."""
    import uuid as uuidlib
    b = open(path, 'rb').read()
    ncmds = struct.unpack_from('<I', b, 16)[0]
    off, uid, sym, text = 32, None, None, 0
    for _ in range(ncmds):
        cmd, cs = struct.unpack_from('<II', b, off)
        if cmd == 0x19 and b[off + 8:off + 14] == b'__TEXT':
            text = struct.unpack_from('<Q', b, off + 24)[0]
        if cmd == 0x1b:
            uid = str(uuidlib.UUID(bytes=b[off + 8:off + 24])).upper()
        elif cmd == 0x2:
            sym = struct.unpack_from('<IIII', b, off + 8)
        off += cs
    syms = []
    if sym:
        symoff, nsyms, stroff, _ = sym
        for k in range(nsyms):
            strx, ntype, sect, desc, value = struct.unpack_from('<IBBHQ', b, symoff + 16 * k)
            if ntype & 0xe0 or sect == 0:
                continue
            syms.append((value, b[stroff + strx:b.index(b'\0', stroff + strx)].decode(errors='replace')))
    syms.sort()
    kernel = struct.unpack_from('<I', b, 12)[0] == 2 and 'kernel' in path.rsplit('/', 1)[-1]
    return ('__kernel__' if kernel else uid), path.rsplit('/', 1)[-1], text, [a for a, _ in syms], \
        [n for _, n in syms]


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    cache_dir, dump = sys.argv[1:3]
    local = {}
    for path in sys.argv[3:]:
        uid, name, text_vm, addrs, names = read_macho(path)
        local[uid] = (name, text_vm, addrs, names)
    shared_start, images = read_images(cache_dir + '/dyld_shared_cache_arm64')
    text = open(dump, errors='replace').read()
    frame = re.compile(r'\?\?\? \(([^ ()]+) \+ (\d+)\)')
    used = {m[0] for m in frame.findall(text) if m[0] in images}
    tables = read_symbols(cache_dir + '/dyld_shared_cache_arm64.symbols', shared_start,
                          {n: images[n][0] for n in used})
    sys.stderr.write('images in the dump: %d, with symbols: %d\n' % (len(used), len(tables)))

    def name(m):
        img, off = m.group(1), int(m.group(2))
        t = tables.get(img)
        if not t:
            return m.group(0)
        addr = images[img][0] + off
        i = bisect.bisect_right(t[0], addr) - 1
        if i < 0:
            return m.group(0)
        return '%s`%s + %d' % (img, t[1][i], addr - t[0][i])

    text = frame.sub(name, text)

    def name_local(m):
        uid, off = m.group(1), int(m.group(2))
        if uid not in local:
            return m.group(0)
        img, text_vm, addrs, names = local[uid]
        off += text_vm      # dylibs link at 0; the kernel at its __TEXT address
        i = bisect.bisect_right(addrs, off) - 1
        if i < 0:
            return '%s + %d' % (img, off)
        return '%s`%s + %d' % (img, names[i], off - addrs[i])

    sys.stdout.write(re.sub(r'\?\?\? \(<?([0-9A-F-]+|__kernel__)>? \+ (\d+)\)', name_local, text))


if __name__ == '__main__':
    main()
