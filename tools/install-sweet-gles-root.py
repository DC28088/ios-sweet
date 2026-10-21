#!/usr/bin/env python3
"""Install/restore sweet GLES in an explicitly mounted iOS16 root. Never flashes a partition."""
from pathlib import Path
import argparse,hashlib,json,os,plistlib,shutil,struct
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--root',required=True,type=Path)
p.add_argument('--bundle',type=Path,default=Path(__file__).resolve().parent)
p.add_argument('--action',choices=['inspect','install','rollback'],default='inspect')
a=p.parse_args();root=a.root.resolve();bundle=a.bundle.resolve()
if root==Path('/') or not root.is_dir():raise SystemExit('Supply the mounted iOS root, not the host root')
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
version_path=root/'System/Library/CoreServices/SystemVersion.plist'
version=plistlib.loads(version_path.read_bytes())
if version.get('ProductVersion')!='16.7.16' or version.get('ProductBuildVersion')!='20H392':raise SystemExit('Expected iOS16.7.16 /20H392')
cache=root/'System/Library/Caches/com.apple.dyld/dyld_shared_cache_arm64'
profile=json.loads((bundle/'cache-profile.json').read_text())
with cache.open('rb') as f:head=f.read(104)
if not head.startswith(b'dyld_v') or head[88:104].hex()!=profile['cache_uuid']:raise SystemExit('Installed dyld cache UUID does not match this GLES build')
dest=root/'usr/lib/sweetgl.dylib';backup=root/'usr/lib/sweetgl.dylib.pre-sweet-gles'
state=root/'private/var/db/sweet-gles-install.json'
if any(x.is_symlink() for x in [dest,backup,state]):raise SystemExit('Refusing symbolic-link installation targets')
if a.action=='inspect':
 print(json.dumps({'ios':version['ProductVersion'],'build':version['ProductBuildVersion'],'cache_uuid':profile['cache_uuid'],
                  'library_installed':dest.exists(),'record_exists':state.exists(),'boot_mode_required':'sweetgles=1 with kernel0115 or newer'},indent=2));raise SystemExit(0)
if a.action=='install':
 report=json.loads((bundle/'build.json').read_text());source=bundle/'sweetgl.dylib'
 expected=report['files']['sweetgl.dylib']
 if sha(source)!=expected:raise SystemExit('Dylib checksum mismatch')
 head=source.read_bytes()[:32]
 if len(head)!=32 or struct.unpack_from('<II',head)!= (0xfeedfacf,0x100000c):raise SystemExit('Expected arm64 Mach-O dylib')
 if state.exists():
  old=json.loads(state.read_text())
  if old['installed_sha256']==expected and dest.exists() and sha(dest)==expected:
   print('Matching library already installed. Use the matching GLES boot image.');raise SystemExit(0)
  raise SystemExit('An earlier GLES installation exists. Roll it back first.')
 if backup.exists():raise SystemExit('Unowned backup exists; refusing to overwrite')
 old_sha=sha(dest) if dest.exists() else None
 if old_sha:
  shutil.copy2(dest,backup)
  if sha(backup)!=old_sha:raise SystemExit('Backup checksum mismatch')
 pending=dest.with_name(dest.name+'.pending-sweet-gles')
 if pending.exists():raise SystemExit('Pending file exists; inspect before continuing')
 shutil.copyfile(source,pending);pending.chmod(0o755)
 if os.geteuid()==0:os.chown(pending,0,0)
 with pending.open('rb') as f:os.fsync(f.fileno())
 if sha(pending)!=expected:raise SystemExit('Copied dylib checksum mismatch')
 os.replace(pending,dest)
 state.parent.mkdir(parents=True,exist_ok=True)
 state.write_text(json.dumps({'installed_sha256':expected,'previous_sha256':old_sha,'cache_uuid':profile['cache_uuid'],
                             'boot_mode':'sweetgles=1','mesa':report['mesa'],'reference':report['reference']},indent=2)+'\n')
 if os.geteuid()==0:os.chown(state,0,0)
 os.sync()
 print('Installed sweetgl.dylib; original saved if present. Boot the matching GLES kernel with sweetgles=1.')
else:
 record=json.loads(state.read_text())
 if not dest.exists() or sha(dest)!=record['installed_sha256']:raise SystemExit('Current dylib changed since install; refusing rollback')
 if record['previous_sha256']:
  if not backup.exists() or sha(backup)!=record['previous_sha256']:raise SystemExit('Original backup missing or changed')
  os.replace(backup,dest)
 else:dest.unlink()
 state.unlink();os.sync();print('Restored the original library state. Boot without sweetgles=1 for the original Metal path.')
