#!/usr/bin/env python3
"""Files on the running phone over the USB cable (xnu 0168 bsd/dev/sweet_files.c, boot-arg sweetfiles=1,
TCP 192.168.7.2:2223). From ios-flame/sunfish tools/flame-files.py (their READ range command is not served).

  tools/sweet-files.py put LOCAL REMOTE [MODE]     (mode octal, default from LOCAL; root:wheel)
  tools/sweet-files.py get REMOTE LOCAL
  tools/sweet-files.py mkdir REMOTE [MODE]
  tools/sweet-files.py link TARGET REMOTE          (symlink)
  tools/sweet-files.py rm REMOTE
  tools/sweet-files.py chmod REMOTE MODE
  tools/sweet-files.py sync                        (flush before a forced reboot)
"""
import os, socket, stat, sys

HOST, PORT = os.environ.get('PHONE_FILES_HOST', '192.168.7.2'), 2223


def conn():
    s = socket.create_connection((HOST, PORT), timeout=30)
    return s


def reply(s):
    line = b''
    while not line.endswith(b'\n'):
        c = s.recv(1)
        if not c:
            raise SystemExit('connection closed without a reply')
        line += c
    line = line.decode().strip()
    if not line.startswith('OK'):
        raise SystemExit(f'phone: {line}')
    return line


def main(argv):
    if not argv:
        raise SystemExit(__doc__)
    cmd, args = argv[0], argv[1:]
    s = conn()
    if cmd == 'put':
        local, remote = args[0], args[1]
        mode = int(args[2], 8) if len(args) > 2 else stat.S_IMODE(os.stat(local).st_mode)
        data = open(local, 'rb').read()
        s.sendall(f'PUT {remote} {mode:o} {len(data)}\n'.encode() + data)
        print(remote, reply(s))
    elif cmd == 'get':
        s.sendall(f'GET {args[0]}\n'.encode())
        n = int(reply(s).split()[1])
        buf = bytearray()
        while len(buf) < n:
            chunk = s.recv(min(65536, n - len(buf)))
            if not chunk:
                raise SystemExit('short read')
            buf += chunk
        open(args[1], 'wb').write(buf)
        print(args[0], n, 'bytes ->', args[1])
    elif cmd == 'sync':
        s.sendall(b'SYNC\n')
        print('sync', reply(s))
    elif cmd in ('mkdir', 'link', 'rm', 'chmod'):
        verb = {'mkdir': 'MKDIR', 'link': 'LINK', 'rm': 'RM', 'chmod': 'CHMOD'}[cmd]
        if cmd == 'mkdir' and len(args) == 1:
            args = args + ['755']
        s.sendall(f'{verb} {" ".join(args)}\n'.encode())
        print(cmd, ' '.join(args), reply(s))
    else:
        raise SystemExit(__doc__)


if __name__ == '__main__':
    main(sys.argv[1:])
