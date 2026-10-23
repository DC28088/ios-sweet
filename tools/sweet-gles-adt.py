#!/usr/bin/env python3
"""Retain a firmware-bearing Sweet ADT, advertising only supported GLES colour."""
import argparse
from pathlib import Path
import adt
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('input',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
if a.output.exists():raise SystemExit('Refusing to overwrite ADT')
blob=a.input.read_bytes();root,end=adt.parse(blob)
if end!=len(blob):raise SystemExit('Unexpected trailing ADT bytes')
product=next((n for n in root.children if n.name=='product'),None)
if product is None:raise SystemExit('No product identity')
changes=0
for i,(key,flags,value) in enumerate(product.props):
 if key in ('artwork-display-gamut','display-gamut'):
  product.props[i]=(key,flags,b'sRGB\0');changes+=1
if not changes:raise SystemExit('No gamut property found')
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(adt.serialize(root))
print('Preserved firmware ADT; set',changes,'gamut properties to sRGB:',a.output)
