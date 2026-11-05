#!/usr/bin/env python3
"""Inspect/repair the observed invalid ADI container template in a mounted iOS root.

Only com.apple.adid's five standard directories are eligible. No account files,
container root permissions, container metadata, or other services are changed.
Default is inspect-only. Apply requires an external, newly created backup file.
"""
import argparse
import json
import os
from pathlib import Path
import plistlib
import stat

IDENTIFIER = 'com.apple.adid'
DIRECTORIES = ('Documents', 'Library', 'Library/Caches', 'Library/Preferences', 'tmp')
MOBILE = 501
INVALID_OWNER = 4294967294


def inspect(root):
    if not (root / 'usr/libexec/adid').is_file():
        raise ValueError('Root must contain usr/libexec/adid')
    base = root / 'private/var/containers/Data/System'
    matches = []
    for metadata in base.glob('*/.com.apple.mobile_container_manager.metadata.plist'):
        if metadata.is_symlink() or not metadata.resolve().is_relative_to(root):
            continue
        with metadata.open('rb') as stream:
            data = plistlib.load(stream)
        if data.get('MCMMetadataIdentifier') == IDENTIFIER:
            matches.append(metadata.parent)
    if len(matches) != 1:
        raise ValueError(f'Expected exactly one ADI container; found {len(matches)}')
    container = matches[0]
    rows = []
    for relative in DIRECTORIES:
        path = container / relative
        info = path.lstat()
        if not stat.S_ISDIR(info.st_mode) or not path.resolve().is_relative_to(root):
            raise ValueError(f'{relative}: expected a directory inside the mounted root')
        mode = stat.S_IMODE(info.st_mode)
        damaged = info.st_uid == INVALID_OWNER and info.st_gid == INVALID_OWNER and mode == 0
        repaired = info.st_uid == MOBILE and info.st_gid == MOBILE and mode == 0o700
        rows.append({'relative': relative, 'uid': info.st_uid, 'gid': info.st_gid,
                     'mode': mode, 'action': 'repair' if damaged else 'already repaired' if repaired else 'unrecognized'})
    return container, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True, help='Mounted iOS root directory')
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--backup', type=Path, help='New private JSON file outside the mounted root')
    args = parser.parse_args()
    root = args.root.resolve(strict=True)
    container, rows = inspect(root)
    for row in rows:
        print(f"{row['relative']}: uid={row['uid']} gid={row['gid']} mode={row['mode']:03o}: {row['action']}")
    if not args.apply:
        return
    if any(row['action'] == 'unrecognized' for row in rows):
        raise ValueError('Unexpected permissions: no changes made')
    targets = [row for row in rows if row['action'] == 'repair']
    if not targets:
        print('No repair needed')
        return
    if args.backup is None:
        raise ValueError('--apply requires --backup')
    backup = args.backup.resolve()
    if backup.is_relative_to(root):
        raise ValueError('Backup must be outside the mounted root')
    fd = os.open(backup, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'w') as stream:
        json.dump({'identifier': IDENTIFIER, 'container_relative': str(container.relative_to(root)),
                   'directories': rows}, stream, indent=2)
        stream.write('\n')
    changed = []
    try:
        for row in targets:
            path = container / row['relative']
            # Recheck immediately before mutation; refuse changed types/permissions.
            info = path.lstat()
            if not stat.S_ISDIR(info.st_mode) or (info.st_uid, info.st_gid, stat.S_IMODE(info.st_mode)) != (row['uid'], row['gid'], row['mode']):
                raise ValueError(f"{row['relative']}: metadata changed since inspection")
            changed.append(row)
            os.chown(path, MOBILE, MOBILE, follow_symlinks=False)
            os.chmod(path, 0o700, follow_symlinks=False)
            check = path.lstat()
            if (check.st_uid, check.st_gid, stat.S_IMODE(check.st_mode)) != (MOBILE, MOBILE, 0o700):
                raise RuntimeError(f"{row['relative']}: repair verification failed")
    except Exception:
        for row in reversed(changed):
            path = container / row['relative']
            os.chown(path, row['uid'], row['gid'], follow_symlinks=False)
            os.chmod(path, row['mode'], follow_symlinks=False)
        raise
    print(f'Repaired {len(targets)} standard ADI directories; contents unchanged')


if __name__ == '__main__':
    main()
