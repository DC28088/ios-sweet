#!/usr/bin/env python3
"""Read Sweet's firmware over ADB into an ignored, local directory.

Recovery example (mount source partitions read-only first):
  pull-sweet-firmware.py --vendor-root /tmp/sweet-fw-vendor \
    --modem-root /tmp/sweet-fw-modem/image research/sweet-firmware-20261023
No firmware is installed or flashed. Wi-Fi credentials/MAC files are excluded.
"""
import argparse, fnmatch, hashlib, io, json, shlex, subprocess, tarfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path)
p.add_argument('--vendor-root',default='/vendor')
p.add_argument('--modem-root',default='/vendor/firmware_mnt/image')
a=p.parse_args()
if a.output.exists():raise SystemExit('Refusing to overwrite an existing firmware collection')
rules={
 'gpu':(a.vendor_root+'/firmware',['a618_gmu.bin','a630_sqe.fw','a615_zap.mdt','a615_zap.b[0-9][0-9]','a615_zap.elf']),
 'audio':(a.modem_root,['adsp.mdt','adsp.b[0-9][0-9]','adsp*.jsn']),
 'wifi':(a.modem_root,['wlanmdsp.mbn','bdwlan.*']),
 'modem':(a.modem_root,['modem.mdt','modem.b[0-9][0-9]']),
 'haptic':(a.vendor_root+'/firmware',['aw8624_haptic.bin','aw8624_rtp.bin']),
}
selected={}
for group,(base,patterns) in rules.items():
 names=subprocess.check_output(['adb','shell','ls -1 '+shlex.quote(base)],text=True).replace('\r','').splitlines()
 for name in names:
  if '/' in name or name in ('.','..'):continue
  if any(fnmatch.fnmatchcase(name,pattern) for pattern in patterns):selected[base+'/'+name]=(group,name)
required={'a618_gmu.bin','a630_sqe.fw','a615_zap.mdt','adsp.mdt','wlanmdsp.mbn','aw8624_haptic.bin'}
missing=required-{name for _,name in selected.values()}
if missing:raise SystemExit('Missing Sweet firmware: '+', '.join(sorted(missing)))
paths=sorted(selected)
remote=subprocess.check_output(['adb','shell','sha256sum '+' '.join(map(shlex.quote,paths))],text=True)
digests={line.split()[1]:line.split()[0] for line in remote.replace('\r','').splitlines()}
archive=subprocess.check_output(['adb','exec-out','tar -C / -c -f - '+' '.join(shlex.quote(path.lstrip('/')) for path in paths)])
a.output.mkdir(parents=True)
manifest=[]
with tarfile.open(fileobj=io.BytesIO(archive),mode='r:') as t:
 for member in t:
  path='/'+member.name.lstrip('/')
  if path not in selected or not member.isfile():raise SystemExit('Unexpected firmware tar entry')
  data=t.extractfile(member).read();digest=hashlib.sha256(data).hexdigest()
  if digest!=digests.get(path):raise SystemExit('Firmware transfer hash mismatch: '+path)
  group,name=selected[path];out=a.output/group/name;out.parent.mkdir(exist_ok=True);out.write_bytes(data)
  manifest.append({'group':group,'file':name,'source':path,'bytes':len(data),'sha256':digest})
if len(manifest)!=len(paths):raise SystemExit('Incomplete firmware transfer')
(a.output/'manifest.json').write_text(json.dumps({'device':'sweet','private_files':True,'files':manifest},indent=2)+'\n')
print('Verified',len(manifest),'firmware files,',sum(x['bytes'] for x in manifest),'bytes in',a.output)
