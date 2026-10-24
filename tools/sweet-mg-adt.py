#!/usr/bin/env python3
"""Add missing MobileGestalt MAC identities to an existing firmware-bearing ADT."""
import argparse
import hashlib
import json
from pathlib import Path
import adt
from mkadt import SWEET_MG_MACS

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('input', type=Path)
parser.add_argument('output', type=Path)
a = parser.parse_args()
if a.output.exists():
    raise SystemExit('Refusing to overwrite ADT')
blob = a.input.read_bytes()
root, end = adt.parse(blob)
if end != len(blob):
    raise SystemExit('Unexpected trailing ADT bytes')
chosen = next((n for n in root.children if n.name == 'chosen'), None)
if chosen is None:
    raise SystemExit('Missing chosen node')
added = []
for key, value in SWEET_MG_MACS.items():
    existing = [(flags, data) for name, flags, data in chosen.props if name == key]
    if existing:
        if len(existing) != 1 or existing[0][0] or len(existing[0][1]) != 6:
            raise SystemExit('Invalid existing MAC property: ' + key)
        continue  # Preserve an existing per-device identity.
    chosen.props.append((key, 0, value))
    added.append(key)
output = adt.serialize(root)
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_bytes(output)
print(json.dumps({'input_sha256': hashlib.sha256(blob).hexdigest(),
                  'output_sha256': hashlib.sha256(output).hexdigest(),
                  'added': added, 'existing_identities_preserved': True}))
