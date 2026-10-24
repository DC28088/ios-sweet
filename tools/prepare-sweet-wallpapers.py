#!/usr/bin/env python3
"""Prepare PNG wallpaper layers in a separate bundle; never modify an iOS root.

Based on joranv1/ios-sailfish tools/wallpaper-heic2png.sh: the current port has
no HEVC image decoder. Keep original HEICs and redirect CAML image sources to
PNG copies. Requires libheif's heif-convert with an HEVC decoder on the host.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import urllib.parse
import zlib


def sha(data):
    return hashlib.sha256(data).hexdigest()


def png_info(data):
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Decoder did not produce PNG')
    at = 8
    info = None
    ended = False
    while at < len(data):
        if len(data) - at < 12:
            raise ValueError('Truncated PNG chunk')
        size = struct.unpack_from('>I', data, at)[0]
        end = at + 12 + size
        if end > len(data):
            raise ValueError('Truncated PNG data')
        kind = data[at + 4:at + 8]
        body = data[at + 8:at + 8 + size]
        crc = struct.unpack_from('>I', data, at + 8 + size)[0]
        if zlib.crc32(kind + body) & 0xffffffff != crc:
            raise ValueError('PNG CRC mismatch')
        if info is None:
            if kind != b'IHDR' or size != 13:
                raise ValueError('PNG has no initial IHDR')
            width, height, depth, color, compression, filtering, interlace = struct.unpack('>IIBBBBB', body)
            if not width or not height or depth not in (8, 16) or color not in (0, 2, 4, 6):
                raise ValueError('Unexpected decoded PNG layout')
            info = dict(width=width, height=height, bit_depth=depth, alpha=color in (4, 6))
        if kind == b'tRNS':
            info['alpha'] = True
        at = end
        if kind == b'IEND':
            ended = True
            break
    if not ended or at != len(data):
        raise ValueError('Invalid PNG ending')
    return info


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True, help='Mounted local root, or extracted wallpaper tree')
    parser.add_argument('--output', type=Path, required=True, help='New bundle directory outside the input root')
    parser.add_argument('--converter', default='heif-convert')
    args = parser.parse_args()
    root = args.root.resolve(strict=True)
    output = args.output.resolve()
    if root == Path('/') or output == root or root in output.parents or output in root.parents:
        parser.error('Use separate input/output directories, not the host root')
    if output.exists():
        parser.error('Output exists; refusing to overwrite a bundle')
    collection = root / 'Library/Wallpaper/Collections'
    if not collection.is_dir():
        parser.error('No Library/Wallpaper/Collections in input')
    converter = shutil.which(args.converter)
    if not converter:
        parser.error('heif-convert is unavailable')

    def checked(path):
        resolved = path.resolve(strict=True)
        if root not in resolved.parents or resolved != path:
            raise ValueError('Refusing symlink or out-of-root source: ' + str(path))
        return resolved

    sources = {}
    files = []
    cache = {}
    output.mkdir(parents=True)
    payload = output / 'files'
    for heic in sorted(collection.rglob('*')):
        if not heic.is_file() or heic.suffix.lower() != '.heic':
            continue
        checked(heic)
        relative = heic.relative_to(root)
        source = heic.read_bytes()
        digest = sha(source)
        sources[str(relative)] = digest
        target = relative.with_suffix('.png')
        dest = payload / target
        dest.parent.mkdir(parents=True, exist_ok=True)
        if digest not in cache:
            subprocess.run([converter, '--quiet', '--codec-threads', '2',
                            '--tile-threads', '1', str(heic), str(dest)], check=True, timeout=120)
            png = dest.read_bytes()
            info = png_info(png)
            # HEIF alpha auxiliary-image identifiers. Do not composite the
            # layer against a solid background or silently discard its alpha.
            has_alpha = any(marker in source for marker in
                            (b'urn:mpeg:hevc:2015:auxid:1', b'urn:mpeg:mpegB:cicp:systems:auxiliary:alpha'))
            if has_alpha and not info['alpha']:
                raise ValueError('Decoder dropped alpha: ' + str(relative))
            cache[digest] = (png, info)
        else:
            png, info = cache[digest]
            dest.write_bytes(png)
        old = root / target
        old_digest = sha(checked(old).read_bytes()) if old.exists() else None
        files.append(dict(path=str(target), before_sha256=old_digest,
                          sha256=sha(png), bytes=len(png), png=info))
        print('Prepared', target, info, flush=True)

    source_attr = re.compile(r'''(\bsrc\s*=\s*)(["'])([^"']+\.heic)\2''', re.IGNORECASE)
    for caml in sorted(collection.rglob('*.caml')):
        checked(caml)
        original = caml.read_bytes()
        text = original.decode('utf-8')

        def redirect(match):
            ref = match[3]
            decoded = urllib.parse.unquote(ref)
            if urllib.parse.urlsplit(decoded).scheme or Path(decoded).is_absolute():
                raise ValueError('Unexpected external image reference: ' + ref)
            source = checked(caml.parent / decoded)
            if source.suffix.lower() != '.heic':
                raise ValueError('Unexpected CAML image suffix')
            twin = source.relative_to(root).with_suffix('.png')
            if not (payload / twin).is_file():
                raise ValueError('CAML reference has no prepared PNG: ' + ref)
            return match[1] + match[2] + ref[:-5] + '.png' + match[2]

        modified = source_attr.sub(redirect, text).encode('utf-8')
        if modified == original:
            continue
        relative = caml.relative_to(root)
        sources[str(relative)] = sha(original)
        dest = payload / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(modified)
        files.append(dict(path=str(relative), before_sha256=sha(original),
                          sha256=sha(modified), bytes=len(modified)))

    if not cache or not any(x['path'].endswith('.caml') for x in files):
        raise ValueError('No HEIC wallpaper layers and CAML references were prepared')
    manifest = dict(status='prepared-not-installed', original_heics_retained=True,
                    reference='https://github.com/joranv1/ios-sailfish/blob/ce637521bacc06fab5cc29cf144febee2fca9ff8/tools/wallpaper-heic2png.sh',
                    sources_sha256=sources, files=files, unique_decodes=len(cache))
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Prepared', len(files), 'files;', len(cache), 'unique HEIC decodes. Input unchanged.')


if __name__ == '__main__':
    main()
