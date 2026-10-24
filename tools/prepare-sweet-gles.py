#!/usr/bin/env python3
"""Adapt the pinned Sailfish GLES frontend to sweet; does not build, install or run it."""
from pathlib import Path
import argparse,hashlib,json,re,shutil
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('reference',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
manifest=json.loads((ROOT/'userspace/gles/reference-inputs.json').read_text())
for name,digest in manifest['reference_files_sha256'].items():
 if hashlib.sha256((a.reference/name).read_bytes()).hexdigest()!=digest:raise SystemExit('Reference hash mismatch: '+name)
if a.output.exists():raise SystemExit('Refusing to overwrite prepared sources: '+str(a.output))
shutil.copytree(a.reference/'tools/sweetgl',a.output,ignore=shutil.ignore_patterns('out','__pycache__'))
def edit(name,transform):
 path=a.output/name;path.write_text(transform(path.read_text()))
def once(s,old,new):
 if s.count(old)!=1:raise SystemExit('Unexpected reference source: '+old[:100])
 return s.replace(old,new)
def drm(s):
 s=once(s,'#include <IOKit/IOKitLib.h>','#include <IOKit/IOKitLib.h>\n#include "sweet_native.h"')
 start=s.index('static void\nconnect_dev(void)');end=s.index('\nint\nsweet_drm_open(void)',start)
 s=s[:start]+'''static void
connect_dev(void)
{
    uint64_t info[5];
    if (sweet_gpu_open(&conn, info) != KERN_SUCCESS) { conn=0; return; }
    chip_id=(uint32_t)info[0];gmem_size=(uint32_t)info[1];gmem_base=info[2];max_freq=0;
    dev_fd=open("/dev/null", O_RDWR | O_CLOEXEC);
}
'''+s[end:]
 s=s.replace('IOConnectCallScalarMethod(', 'sweet_gpu_scalar(').replace('IOConnectCallMethod(', 'sweet_gpu_method(')
 s=once(s,'p->value = 530;', 'p->value = ((chip_id >> 24) & 255) * 100 + ((chip_id >> 16) & 255) * 10 + ((chip_id >> 8) & 255);')
 s=once(s,'p->value = max_freq;\n\t\treturn 0;', 'return -EINVAL; /* no qualified frequency counter: do not invent one */')
 s=once(s,'p->value = mach_absolute_time();','p->value = sweet_gpu_timestamp();')
 s=once(s,'case MSM_PARAM_FAULTS:\n\tcase MSM_PARAM_SUSPENDS:\n\t\tp->value = 0;\n\t\treturn 0;', '''case MSM_PARAM_FAULTS:
    case MSM_PARAM_SUSPENDS: {
        uint64_t faults=0,suspends=0;
        if (sweet_gpu_status(&faults,&suspends)) return -EIO;
        p->value=p->param==MSM_PARAM_FAULTS?faults:suspends;
        return 0;
    }''')
 s=once(s,'\tconst struct drm_msm_gem_submit_bo *sb =', '\tif (!r->nr_cmds || r->nr_cmds > 4096 || !r->nr_bos || r->nr_bos > 1024) return -EINVAL;\n\tconst struct drm_msm_gem_submit_bo *sb =')
 # Stop unsupported command types reaching the compatibility transport.
 s=once(s,'\t\tstruct sbo *b = sc[i].submit_idx', '\t\tif (sc[i].type != MSM_SUBMIT_CMD_BUF) { ret=-EINVAL; break; }\n\t\tstruct sbo *b = sc[i].submit_idx')
 s=once(s,'sbo_add((uint32_t)out[0], out[1], map, out[2])', 'sbo_add((uint32_t)out[0], out[1], map & ~16383ull, out[2])')
 s=once(s,'static uint64_t gmem_base, max_freq;', 'static uint64_t gmem_base;')
 s=s.replace('gmem_base=info[2];max_freq=0;', 'gmem_base=info[2];')
 s=once(s,'((struct drm_msm_submitqueue *)data)->id = ++queues;\n\t\treturn 0;', 'struct drm_msm_submitqueue *q=data;\n        if (q->flags || q->prio) return -EINVAL;\n        pthread_mutex_lock(&lock);\n        for (uint32_t id=1;id<8;id++) {\n            if (!(queues & (1u<<id))) {\n                queues|=1u<<id;q->id=id;\n                pthread_mutex_unlock(&lock);return 0;\n            }\n        }\n        pthread_mutex_unlock(&lock);return -ENOSPC;')
 s=once(s,'case DRM_MSM_SUBMITQUEUE_NEW:\n\t\t', 'case DRM_MSM_SUBMITQUEUE_NEW: {\n\t\t')
 s=once(s,'case DRM_MSM_SUBMITQUEUE_CLOSE:\n\t\treturn 0;', '}\n    case DRM_MSM_SUBMITQUEUE_CLOSE: {\n        uint32_t id=*(uint32_t *)data;\n        if (!id || id>=8) return -EINVAL;\n        pthread_mutex_lock(&lock);\n        int active=(queues&(1u<<id))!=0;\n        queues&=~(1u<<id);\n        pthread_mutex_unlock(&lock);\n        return active?0:-EINVAL;\n    }')
 s=once(s,'*(uint32_t *)(uintptr_t)q->data = 0;', 'uint64_t faults=0,suspends=0;\n        if (sweet_gpu_status(&faults,&suspends)) return -EIO;\n        *(uint32_t *)(uintptr_t)q->data = faults>UINT32_MAX?UINT32_MAX:(uint32_t)faults;')
 s=once(s,'if (!r->nr_cmds || r->nr_cmds > 4096 || !r->nr_bos || r->nr_bos > 1024) return -EINVAL;', 'if (!r->nr_cmds || r->nr_cmds > 4096 || !r->nr_bos || r->nr_bos > 1024 || !r->queueid || r->queueid>=8) return -EINVAL;')
 # a6xx resource import validates the layout against the actual BO extent.
 anchor='void *\nsweet_drm_bo_map('
 index=s.index(anchor)
 s=s[:index]+'''uint64_t
sweet_drm_bo_size(int fd, uint32_t handle)
{
    (void)fd;
    pthread_mutex_lock(&lock);
    struct sbo *b=sbo(handle);
    uint64_t size=b?b->size:0;
    static unsigned reports;
    int report=reports++<8;
    pthread_mutex_unlock(&lock);
    if (report) sweet_drm_log("sweetgl: imported BO %u allocation %llu bytes",handle,(unsigned long long)size);
    return size;
}

'''+s[index:]
 # Preserve Mesa's complete per-submit BO read/write set. NativeAdreno uses
 # it both for residency pinning and scoped non-coherent cache ownership.
 s=once(s,'r->nr_cmds * 16 + r->nr_bos * 4','r->nr_cmds * 16 + r->nr_bos * 8')
 s=once(s,'\t\thandles[i] = sb[i].handle;', '''        uint32_t access=sb[i].flags & (MSM_SUBMIT_BO_READ | MSM_SUBMIT_BO_WRITE);
        if (!access || (sb[i].flags & ~(MSM_SUBMIT_BO_READ | MSM_SUBMIT_BO_WRITE | MSM_SUBMIT_BO_DUMP))) ret=-EINVAL;
        handles[2*i] = sb[i].handle;
        handles[2*i+1] = access;''')
 s=once(s,'8 + nib * 16 + r->nr_bos * 4','8 + nib * 16 + r->nr_bos * 8')
 return s
edit('sweet_drm.c',drm)
profile=json.loads((ROOT/'userspace/gles/cache-profile.json').read_text())
def eagl(s):
 delta=profile['quartzcore_address_delta']
 # All original absolute QuartzCore targets, including optional diagnostic hooks,
 # were audited against this exact cache. Never apply these to another cache UUID.
 addresses=set(re.findall(r'0x(?:188[0-9a-f]{6}|1d5b44a10)',s))
 for addr in sorted(addresses,key=len,reverse=True):s=s.replace(addr,hex(int(addr,16)+delta))
 uuid=', '.join('0x'+profile['cache_uuid'][i:i+2] for i in range(0,32,2))
 needle='void\nsweetgl_eagl_install(void)\n{'
 gate='''void
sweetgl_eagl_install(void)
{
    static const unsigned char expected_uuid[16]={UUID};
    size_t cache_length=0;
    const unsigned char *cache=_dyld_get_shared_cache_range(&cache_length);
    if (!cache || cache_length<104 || memcmp(cache+88,expected_uuid,16)) {
        sweet_drm_log("sweetgl: cache UUID mismatch; refusing QuartzCore patches and EAGL hooks");
        return;
    }
'''.replace('{UUID}', '{'+uuid+'}')
 s=once(s,needle,gate)
 # HackiOS eagl.c uses ordinary getters when IOSurface has no plane array.
 # On our16.7 cache OfPlane(s,0) returns zero pitch/BPE for those surfaces.
 old='\tsize_t w = IOSurfaceGetWidthOfPlane(s, plane), h = IOSurfaceGetHeightOfPlane(s, plane);\n\tsize_t pitch = IOSurfaceGetBytesPerRowOfPlane(s, plane);'
 new='\tbool planar = IOSurfaceGetPlaneCount(s) != 0;\n\tif ((!planar && plane != 0) || (planar && plane >= IOSurfaceGetPlaneCount(s))) return false;\n\tsize_t w = planar ? IOSurfaceGetWidthOfPlane(s, plane) : IOSurfaceGetWidth(s);\n\tsize_t h = planar ? IOSurfaceGetHeightOfPlane(s, plane) : IOSurfaceGetHeight(s);\n\tsize_t pitch = planar ? IOSurfaceGetBytesPerRowOfPlane(s, plane) : IOSurfaceGetBytesPerRow(s);\n\tsize_t bpe = planar ? IOSurfaceGetBytesPerElementOfPlane(s, plane) : IOSurfaceGetBytesPerElement(s);\n\tvoid *base = planar ? IOSurfaceGetBaseAddressOfPlane(s, plane) : IOSurfaceGetBaseAddress(s);\n\tif (!w || !h || !pitch || !bpe || !base || w > SIZE_MAX / bpe || pitch < w * bpe) return false;'
 s=once(s,old,new)
 s=once(s,'surface_format(fourcc, IOSurfaceGetBytesPerElementOfPlane(s, plane), &img.internalformat)', 'surface_format(fourcc, bpe, &img.internalformat)')
 # Both the resource key and its first-page offset use this plane's validated base.
 s=s.replace('(uintptr_t)IOSurfaceGetBaseAddressOfPlane(s, plane)', '(uintptr_t)base')
 # A6xx linear layouts include the final 4-row GMEM store block. A caller's
 # tightly sized immutable image cannot be enlarged by the IOSurface driver;
 # upload it to Mesa-owned storage when it is used as a sampled texture.
 anchor='\tuint32_t handle = 0;\n\tuint64_t size = 0;'
 s=once(s,anchor,'''    if (!multi && !strcmp(why,"texImageIOSurface") && h <= SIZE_MAX-3 &&
        pitch <= (SIZE_MAX-((uintptr_t)base & vm_page_mask))/((h+3)&~(size_t)3) &&
        IOSurfaceGetAllocSize(s) < ((uintptr_t)base & vm_page_mask) + pitch*((h+3)&~(size_t)3)) {
        if (!format) upload_format(fourcc,bpe,&format,&type);
        if (format) return copy_surface(s,target,format,type,w,h,pitch,why);
    }
'''+anchor)
 return s
edit('eagl.c',eagl)
def frontend(s):
 start=s.index('__attribute__((constructor)) static void\nsweetgl_load(void)')
 return s[:start]+'''__attribute__((constructor)) static void
sweetgl_load(void)
{
    if (strcmp(getprogname(), "backboardd") != 0) return;
    sweet_drm_log("sweetgl: sweet iOS16 GLES frontend loaded; waiting for A618 provider");
    for (unsigned retry=0;retry<900;retry++) {
        if (sweetgl_init()) {
            sweetgl_eagl_install();
            sweet_drm_log("sweetgl: Mesa screen ready; EAGL installation attempted");
            return;
        }
        if (retry % 50 == 0) sweet_drm_log("sweetgl: waiting for A618 provider (%u seconds)",retry/5);
        usleep(200000);
    }
    sweet_drm_log("sweetgl: A618 provider unavailable after180 seconds; GPU UI rendering unavailable");
}
'''
edit('sweetgl.c',frontend)
shutil.copy2(ROOT/'userspace/gles/sweet_native.cpp',a.output/'sweet_native.cpp')
for path in (ROOT/'userspace/gles/include').iterdir():shutil.copy2(path,a.output/path.name)
(a.output/'provenance.json').write_text(json.dumps({'reference':manifest['commit'],'cache_profile':profile,'adapted_files':['sweet_drm.c','sweetgl.c','eagl.c'],'transport':'sweet NativeAdreno ABI1'},indent=2)+'\n')
print('Prepared sweet GLES frontend:',a.output)
