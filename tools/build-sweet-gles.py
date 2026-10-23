#!/usr/bin/env python3
"""Build Sailfish/Mesa GLES for sweet/iOS16. No install, device I/O or runtime tests."""
from pathlib import Path
import hashlib,json,os,shlex,shutil,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'research/sweet-gles-build'
INPUTS=json.loads((ROOT/'userspace/gles/reference-inputs.json').read_text())
def run(*args,**kwargs):
 print('+',shlex.join(map(str,args)),flush=True)
 return subprocess.run(list(map(str,args)),check=True,**kwargs)
def output(*args):return subprocess.check_output(list(map(str,args)),text=True).strip()
def checkout(url,revision,path):
 if path.exists():raise SystemExit('Refusing to overwrite source checkout: '+str(path))
 path.mkdir(parents=True)
 run('git','init','-q',path);run('git','-C',path,'fetch','--depth','1',url,revision)
 run('git','-C',path,'checkout','-q','--detach','FETCH_HEAD')
 if output('git','-C',path,'rev-parse','HEAD')!=revision:raise SystemExit('Wrong source revision')
if sys.platform!='darwin':raise SystemExit('Build requires macOS with the iPhoneOS SDK')
WORK.mkdir(parents=True,exist_ok=True)
ref=WORK/'sailfish';mesa=WORK/'mesa';front=WORK/'frontend';build=WORK/'mesa-ios'
checkout(INPUTS['repository'],INPUTS['commit'],ref)
checkout(INPUTS['mesa_repository'],INPUTS['mesa_commit'],mesa)
run(sys.executable,ROOT/'tools/prepare-sweet-gles.py',ref,front)
run('git','apply',front/'mesa.patch',cwd=mesa)
run('git','apply',ROOT/'userspace/gles/mesa-platform.patch',cwd=mesa)
run('git','apply',ROOT/'userspace/gles/mesa-iosurface-size.patch',cwd=mesa)
sdk=output('xcrun','--sdk','iphoneos','--show-sdk-path')
clang=output('xcrun','--sdk','iphoneos','--find','clang')
clangxx=output('xcrun','--sdk','iphoneos','--find','clang++')
common=['-arch','arm64','-isysroot',sdk,'-miphoneos-version-min=16.0']
# Meson cross-files are literals, not shell commands. Generate SDK paths at build time.
cross=WORK/'ios-cross.txt'
cross.write_text('[binaries]\n'+''.join(k+' = '+repr(v)+'\n' for k,v in {
 'c':[clang]+common,'cpp':[clangxx]+common,'objc':[clang]+common,
 'ar':output('xcrun','--find','ar'),'strip':output('xcrun','--find','strip'),'pkg-config':'false'}.items())+
 "\n[built-in options]\nc_args = "+repr(['-I'+str(front/'include')])+"\ncpp_args = "+repr(['-I'+str(front/'include')])+
 "\n\n[host_machine]\nsystem = 'darwin'\nsubsystem = 'ios'\ncpu_family = 'aarch64'\ncpu = 'arm64'\nendian = 'little'\n")
args=['--buildtype=release','-Db_ndebug=true','-Ddefault_library=static','-Dplatforms=',
 '-Degl=disabled','-Dglx=disabled','-Dgbm=disabled','-Dgallium-drivers=freedreno','-Dvulkan-drivers=',
 '-Dfreedreno-kmds=msm','-Dllvm=disabled','-Dgles1=disabled','-Dgles2=enabled','-Dopengl=true',
 '-Dzstd=disabled','-Dzlib=disabled','-Dexpat=disabled','-Dxmlconfig=disabled','-Dvalgrind=disabled',
 '-Dlibunwind=disabled','-Dlmsensors=disabled','-Dbuild-tests=false','-Dshader-cache=disabled',
 '-Dperfetto=false','-Dgallium-va=disabled','-Dgallium-rusticl=false','-Dtools=',
 '-Dhtml-docs=disabled','-Ddisplay-info=disabled','-Dteflon=false','--wrap-mode=nofallback']
run('meson','setup',build,mesa,'--cross-file',cross,*args)
libs='''src/mesa/libmesa.a src/gallium/auxiliary/libgallium.a
src/gallium/drivers/freedreno/libfreedreno.a src/gallium/winsys/freedreno/drm/libfreedrenowinsys.a
src/freedreno/drm/libfreedreno_drm.a src/freedreno/ir3/libfreedreno_ir3.a
src/freedreno/ir2/libfreedreno_ir2.a src/freedreno/fdl/libfreedreno_layout.a
src/freedreno/common/libfreedreno_common.a src/freedreno/isa/libir3encode.a
src/freedreno/isa/libir3decode.a src/compiler/isaspec/libisaspec.a
src/freedreno/perfcntrs/libfreedreno_perfcntrs.a
src/compiler/glsl/libglsl.a src/compiler/glsl/glcpp/libglcpp.a src/compiler/nir/libnir.a
src/compiler/spirv/libvtn.a src/compiler/libcompiler.a
src/mesa/glapi/glapi/libglapi_bridge.a src/mesa/glapi/shared-glapi/libglapi.a
src/util/libxmlconfig.a src/util/libmesa_util.a src/util/libmesa_util_simd.a src/util/blake3/libblake3.a
src/c11/impl/libmesa_util_c11.a'''.split()
run('ninja','-C',build,'-j','3',*libs)
o=front/'out';o.mkdir()
(o/'bridge.nm').write_text(output('nm','-g',build/'src/mesa/glapi/glapi/libglapi_bridge.a'))
run(sys.executable,front/'gen-dispatch.py',front/'ios-dispatch.txt',mesa/'include',o/'bridge.nm',o/'dispatch.c')
entries=json.loads((build/'compile_commands.json').read_text())
entry=next(x for x in entries if x['file'].endswith('state_tracker/st_manager.c'))
defines=[x for x in shlex.split(entry['command']) if x.startswith('-D')]
include=[front,front/'include',mesa/'include',mesa/'src',mesa/'src/mesa',mesa/'src/mapi',mesa/'src/gallium/include',
 mesa/'src/gallium/auxiliary',mesa/'src/gallium/drivers',mesa/'src/gallium/winsys',build/'src',build/'src/mesa',build/'src/mapi']
flags=common+['-O2','-Wall','-fvisibility=hidden']+defines+['-I'+str(x) for x in include]
objects=[]
for name in ['sweetgl','sweet_drm','eagl','layerdump','dispatch','sweet_native']:
 src=o/'dispatch.c' if name=='dispatch' else front/(name+('.cpp' if name=='sweet_native' else '.c'))
 obj=o/(name+'.o');objects.append(obj)
 run(clangxx if name=='sweet_native' else clang,*flags,*(['-std=c++17'] if name=='sweet_native' else []),'-c',src,'-o',obj)
(o/'exports.txt').write_text('_sweetgl_init\n_sweetgl_create\n_sweetgl_make_current\n_sweetgl_destroy\n')
out=ROOT/'userspace/out/sweet-gles-ios16'
if out.exists():raise SystemExit('Refusing to overwrite an earlier binary')
out.mkdir(parents=True)
dylib=out/'sweetgl.dylib'
run(clangxx,*common,'-dynamiclib','-Wl,-dead_strip','-install_name','/usr/lib/sweetgl.dylib',
 '-exported_symbols_list',o/'exports.txt','-o',dylib,*objects,*[build/x for x in libs],
 '-framework','IOKit','-framework','CoreFoundation','-framework','IOSurface','-framework','OpenGLES',
 '-framework','CoreGraphics','-lobjc','-lc++','-lz')
run('codesign','--force','--sign','-','--pagesize','4096',dylib)
run('codesign','--verify','--strict',dylib)
(out/'dependencies.txt').write_text(output('otool','-L',dylib)+'\n')
(out/'load-commands.txt').write_text(output('otool','-l',dylib)+'\n')
shutil.copy2(ROOT/'userspace/gles/cache-profile.json',out/'cache-profile.json')
shutil.copy2(ROOT/'userspace/gles/reference-inputs.json',out/'reference-inputs.json')
shutil.copy2(ROOT/'tools/install-sweet-gles-root.py',out/'install-sweet-gles-root.py')
(out/'NOTICE.txt').write_text('Sailfish GLES frontend: https://github.com/joranv1/ios-sailfish at '+INPUTS['commit']+'\nMesa freedreno: '+INPUTS['mesa_repository']+' at '+INPUTS['mesa_commit']+'\nOriginal iOS port: dipa-studio/ios-sweet\nSweet transport adaptation: DC28088/ios-sweet\n')
for name in ['docs/license.rst','COPYING','LICENSE']:
 if (mesa/name).exists():shutil.copy2(mesa/name,out/('Mesa-'+Path(name).name))
report={'reference':INPUTS['commit'],'mesa':INPUTS['mesa_commit'],'deployment_target':'16.0','architecture':'arm64',
 'kernel_abi':'NativeAdreno1','kernel_boot_mode':'sweetgles=1','cache_uuid':json.loads((out/'cache-profile.json').read_text())['cache_uuid'],
 'built':True,'installed':False,'hardware_qualified':False,'runtime_tests_run':False,
 'files':{str(p.relative_to(out)):hashlib.sha256(p.read_bytes()).hexdigest() for p in out.iterdir() if p.is_file()}}
(out/'build.json').write_text(json.dumps(report,indent=2)+'\n')
(out/'SHA256SUMS').write_text(''.join(v+'  '+k+'\n' for k,v in report['files'].items()))
print('Built sweet GLES userspace:',out,flush=True)
