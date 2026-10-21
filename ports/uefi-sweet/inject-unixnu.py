#!/usr/bin/env python3
"""Embed the compiled UniXNU app/registrar in the pinned legacy sweet FV."""
import argparse, hashlib, json, re, shutil, struct
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('firmware',type=Path);p.add_argument('menu',type=Path)
a=p.parse_args();fw=a.firmware.resolve();menu=a.menu.resolve()
base=fw/'Platforms/Xiaomi/sweetPkg';out=base/'PrebuiltUniXNU';out.mkdir(exist_ok=True)
for name in ('UniXNU.efi','UniXNUDxe.efi'):
 data=(menu/name).read_bytes()
 if data[:2]!=b'MZ':raise SystemExit(f'{name}: missing PE header')
 offset=struct.unpack_from('<I',data,0x3c)[0]
 if data[offset:offset+4]!=b'PE\0\0' or struct.unpack_from('<H',data,offset+4)[0]!=0xaa64:
  raise SystemExit(f'{name}: expected ARM64 EFI')
 shutil.copyfile(menu/name,out/name)
# Preserve the registrar's two variable-service dependencies. Pull GUID values
# from this exact firmware's MdePkg, rather than embedding platform addresses.
dec=(fw/'Mu_Basecore/MdePkg/MdePkg.dec').read_text()
def guid(name):
 m=re.search(r'\b'+re.escape(name)+r'\s*=\s*(\{[^\n]+)',dec)
 if not m:raise SystemExit(f'Missing GUID: {name}')
 nums=[int(x,16) for x in re.findall(r'0x[0-9a-fA-F]+',m[1])]
 if len(nums)!=11:raise SystemExit(f'Unexpected GUID form: {name}')
 return struct.pack('<IHH8B',*nums)
(out/'UniXNUDxe.depex').write_bytes(b'\x02'+guid('gEfiVariableArchProtocolGuid')+b'\x02'+guid('gEfiVariableWriteArchProtocolGuid')+b'\x03\x08')
fdf=base/'sweet.fdf';source=fdf.read_text();anchor='  !include QcomPkg/Extra.fdf.inc'
if source.count(anchor)!=1 or 'PrebuiltUniXNU' in source:raise SystemExit('Unexpected or already modified sweet FDF')
files='''
  FILE APPLICATION = 9B665204-A133-47A2-9396-9E2B370A4E25 {
    SECTION PE32 = Platforms/Xiaomi/sweetPkg/PrebuiltUniXNU/UniXNU.efi
    SECTION UI = "UniXNU"
  }
  FILE DRIVER = 29B950CF-586D-49EA-9D71-F88645CCDAF6 {
    SECTION DXE_DEPEX = Platforms/Xiaomi/sweetPkg/PrebuiltUniXNU/UniXNUDxe.depex
    SECTION PE32 = Platforms/Xiaomi/sweetPkg/PrebuiltUniXNU/UniXNUDxe.efi
    SECTION UI = "UniXNUDxe"
  }
'''
fdf.write_text(source.replace(anchor,anchor+'\n'+files))
(out/'inputs.json').write_text(json.dumps({n:hashlib.sha256((out/n).read_bytes()).hexdigest() for n in ('UniXNU.efi','UniXNUDxe.efi','UniXNUDxe.depex')},indent=2)+'\n')
print('Embedded UniXNU application and registrar in sweet FvMain source; firmware compilation remains required.')
