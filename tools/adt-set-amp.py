#!/usr/bin/env python3
"""Give an existing ADT's /cpus nodes the SM7150 AMP topology for xnu 0248 (__ARM_AMP__, sched_amp).

    tools/adt-set-amp.py IN.adt OUT.adt [--smp]

cpu0-5 (Cortex-A55): cluster-type 'E', cluster-id 0; cpu6-7 (Cortex-A76): cluster-type 'P', cluster-id 1.
ml_parse_cpu_topology reads cluster-type as a number and compares it with 'E' / 'P'; without HAS_CLUSTER
the logical cluster is derived from the type alone, so cluster-id is descriptive. --smp writes the old
homogeneous values (type 0, id 0) back. Only these two existing 4-byte values change (sizes are kept).
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import adt  # noqa: E402


def u32(v):
    return struct.pack('<I', v)


def setprop(node, key, value):
    for i, (k, f, v) in enumerate(node.props):
        if k == key:
            assert len(v) == len(value), (node.name, key, len(v))
            node.props[i] = (k, f, value)
            return v
    raise SystemExit('%s has no %s' % (node.name, key))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    smp = '--smp' in sys.argv[1:]
    if len(args) != 2:
        raise SystemExit(__doc__)
    src, dst = args
    blob = open(src, 'rb').read()
    root, end = adt.parse(blob)
    assert end == len(blob), 'trailing bytes'
    cpus = [c for c in root.children if c.name == 'cpus']
    assert len(cpus) == 1, 'no single /cpus node'
    nodes = [c for c in cpus[0].children if c.name.startswith('cpu')]
    assert len(nodes) == 8, 'expected 8 cpu nodes, found %d' % len(nodes)
    for n in nodes:
        core = int.from_bytes(n.prop('reg'), 'little') >> 8   # MPIDR Aff1 = physical core
        big = core >= 6
        ctype = 0 if smp else ord('P' if big else 'E')
        cid = 0 if smp else (1 if big else 0)
        old_t = setprop(n, 'cluster-type', u32(ctype))
        old_i = setprop(n, 'cluster-id', u32(cid))
        print('%s core %d: cluster-type %#x -> %#x, cluster-id %d -> %d' % (
            n.name, core, int.from_bytes(old_t, 'little'), ctype, int.from_bytes(old_i, 'little'), cid))
    out = adt.serialize(root)
    assert len(out) == len(blob), 'size changed'
    open(dst, 'wb').write(out)
    print('wrote', dst, len(out), 'bytes')


if __name__ == '__main__':
    main()
