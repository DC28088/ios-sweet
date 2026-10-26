#!/usr/bin/env python3
"""Self (exclusive) sample counts per function for the threads of a symbolicated spindump.

    tools/spindump-self.py SPINDUMP.sym.txt PROCESS [THREAD-NAME-SUBSTRING] [TOP]

Reads spindump's heavy format (each frame line: indentation, count, frame); a frame's self count is its
count less its children's. Threads are matched by name; without a thread name, every thread of the
process. Prints the top functions by self count, then by inclusive count.
"""
import collections
import os
import re
import sys

RUNNING_ONLY = os.environ.get('RUNNING_ONLY') == '1'   # count blocked leaves as "<blocked>"


def main():
    path, proc = sys.argv[1], sys.argv[2]
    want = sys.argv[3] if len(sys.argv) > 3 else ''
    top = int(sys.argv[4]) if len(sys.argv) > 4 else 25
    selfc, incl = collections.Counter(), collections.Counter()
    in_proc = in_thread = False
    stack = []          # (indent, count, name)

    def close(upto):
        while stack and stack[-1][0] >= upto:
            ind, cnt, name, kids = stack.pop()
            selfc[name] += cnt - kids
            if stack:
                stack[-1][3] += cnt

    for line in open(path, errors='replace'):
        if line.startswith('Process:'):
            close(0)
            in_proc = proc in line
            in_thread = False
            continue
        if not in_proc:
            continue
        if line.startswith('  Thread 0x'):
            close(0)
            in_thread = want in line
            continue
        if not in_thread:
            continue
        m = re.match(r'^( +)\*?(\d+) +(.*?)(?: \[0x[0-9a-f]+\])?\s*$', line)
        if not m or len(m.group(1)) < 3:
            continue
        ind, cnt, name = len(m.group(1)), int(m.group(2)), m.group(3).strip()
        if RUNNING_ONLY and re.search(r'\((blocked|suspended)', name):
            name = '<blocked>'
        # leaf frames carry their state and address: "... + 12 [0x1e84c27a8] (running)"
        name = re.sub(r' \[0x[0-9a-f]+\].*$', '', name)
        name = re.sub(r' \((running|blocked[^)]*|suspended)\)$', '', name)
        # an unnamed frame keeps its image ("??? (SweetMetal2)"); a named one loses its offset
        name = re.sub(r'^\?\?\? \(([^ ()]+) \+ \d+\)$', r'??? (\1)', name)
        name = re.sub(r' \+ \d+$', '', name)
        close(ind)
        incl[name] += cnt
        stack.append([ind, cnt, name, 0])
    close(0)
    total = sum(selfc.values()) or 1
    print('self samples: %d' % total)
    for n, c in selfc.most_common(top):
        print('%6d %5.1f%%  %s' % (c, 100.0 * c / total, n[:150]))


if __name__ == '__main__':
    main()
