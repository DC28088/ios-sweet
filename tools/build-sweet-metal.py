#!/usr/bin/env python3
"""Build iOS16 Turnip + metal2vulkan + native Metal bundle on macOS; never install or run it."""
from pathlib import Path
import hashlib,json,os,plistlib,shutil,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'research/native-metal-build'
UNIXNU='ccd8b95f486aa12925cde2d24f0c1f69b57aa9da'
def run(*args,**kwargs):
 subprocess.run([str(x) for x in args],check=True,**kwargs)
def checkout(url,revision,path):
 if path.exists():raise SystemExit('Refusing to reuse an existing build checkout: '+str(path))
 path.parent.mkdir(parents=True,exist_ok=True)
 run('git','init','-q',path)
 run('git','-C',path,'fetch','--depth=1',url,revision)
 run('git','-C',path,'checkout','-q','--detach','FETCH_HEAD')
 actual=subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()
 if actual!=revision:raise SystemExit('Source revision mismatch')
if sys.platform!='darwin':raise SystemExit('This build requires macOS and the iPhoneOS SDK')
checkout('https://github.com/UniXNU/UniXNU.git',UNIXNU,WORK)
run('git','-C',WORK,'submodule','update','--init','--depth=1','drivers/NativeAdreno','drivers/NativeMetal','kits/AdrenoPM4')
# Keep source pins from the build scripts themselves: the compiler manifest is
# the authoritative Mesa base (85fda3b), not the separate GPU survey manifest.
compiler=json.loads((WORK/'scripts/data/adreno-compiler-inputs.json').read_text())
translator=json.loads((WORK/'scripts/data/gpu-metal-vulkan-inputs.json').read_text())['translator']
mesa=WORK/'.cache/mesa-work'
checkout(compiler['mesa']['mirror'],compiler['mesa']['commit'],mesa)
for patch in compiler['patches']:run('git','apply',WORK/patch,cwd=mesa)
checkout(translator['repository'],translator['commit'],WORK/'.cache/gpu-reference/metal2vulkan')
run(sys.executable,'-m','venv',WORK/'.cache/mesa-venv')
python=WORK/'.cache/mesa-venv/bin/python'
run(python,'-m','pip','install','-r',WORK/'scripts/requirements-adreno-compiler.txt')
# The pinned build scripts use this source for iOS17. Set the deployment target
# consistently across Turnip, Rust bridge and the Objective-C frontend.
for name in ('build-turnip-darwin.py','build-native-metal-bridge.py'):
 p=WORK/'scripts'/name;s=p.read_text().replace('arm64-apple-ios17.0','arm64-apple-ios16.0').replace("IPHONEOS_DEPLOYMENT_TARGET='17.0'","IPHONEOS_DEPLOYMENT_TARGET='16.0'")
 p.write_text(s)
frontend=WORK/'drivers/NativeMetal/NativeBridgeMetalFrontend.mm'
s=frontend.read_text()
needle='class_getInstanceSize(base)!=536'
if s.count(needle)!=1:raise SystemExit('Unexpected frontend ABI guard')
# The dynamic subclass adds zero ivars and reuses the actual runtime base.
# 528 comes from this firmware's recorded class_ro, not an invented offset.
s=s.replace(needle,'class_getInstanceSize(base)!=528')
frontend.write_text(s)
run(python,WORK/'scripts/build-turnip-darwin.py','--target','ios')
run(python,WORK/'scripts/build-native-metal-bridge.py','--target','ios')
out=ROOT/'userspace/out/NativeAdrenoMetal.bundle'
if out.exists():raise SystemExit('Refusing to overwrite a prior bundle')
out.mkdir(parents=True)
sdk=subprocess.check_output(['xcrun','--sdk','iphoneos','--show-sdk-path'],text=True).strip()
bridge=WORK/'build/native-metal-bridge/ios/libnative_metal_bridge.a'
exe=out/'NativeAdrenoMetal'
run('xcrun','clang++','-target','arm64-apple-ios16.0','-isysroot',sdk,'-O2','-g','-std=c++17','-fobjc-arc','-bundle',
    '-I'+str(WORK/'drivers/NativeMetal'),'-I'+str(WORK/'drivers/NativeMetal/bridge'),
    ROOT/'userspace/metal/SweetAdrenoMetalPlugin.mm',frontend,bridge,'-Wl,-dead_strip','-lobjc',
    '-framework','Foundation','-framework','Metal','-framework','IOSurface','-o',exe)
run('strip','-x',exe)
shutil.copy2(WORK/'build/mesa-tu-ios/src/freedreno/vulkan/libvulkan_freedreno.dylib',out/'libvulkan_freedreno.dylib')
(out/'Info.plist').write_bytes(plistlib.dumps(dict(CFBundleIdentifier='org.sweet.NativeAdrenoMetal',CFBundleExecutable='NativeAdrenoMetal',
 CFBundlePackageType='BNDL',CFBundleVersion='1',MinimumOSVersion='16.0',NSPrincipalClass='SweetAdrenoMetalDevice')))
for p in (out/'libvulkan_freedreno.dylib',exe):run('codesign','--force','--sign','-','--pagesize','4096',p)
run('codesign','--force','--sign','-','--pagesize','4096',out)
report={'unixnu':UNIXNU,'mesa':compiler['mesa']['commit'],'translator':translator['commit'],'deployment_target':'16.0',
 'runtime_qualified':False,'installed':False,'hardware_tested':False,'files':{str(p.relative_to(out)):hashlib.sha256(p.read_bytes()).hexdigest() for p in out.rglob('*') if p.is_file()}}
(out.parent/'metal-build.json').write_text(json.dumps(report,indent=2)+'\n')
print('Metal userspace compilation complete; bundle not installed or runtime-qualified.')
