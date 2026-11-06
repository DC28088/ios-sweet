#!/usr/bin/env python3
"""Execute actual patched PCM entry points with host kernel/VFS stubs."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
p=(root/'xnu/patches/0215-sweet-coreaudio-pcm-ring.patch').read_text().split('diff --git a/bsd/dev/sweet_pcm_ring.h',1)[0]
s='\n'.join(l[1:] for l in p.splitlines() if l.startswith('+') and not l.startswith('+++'))
a=s.index('static int pcm_dev_open(');b=s.index('static const struct cdevsw',a);functions=s[a:b].replace("user_ssize_t","probe_ssize_t")
assert 'makedev(major,0)' in s and 'makedev(major,1)' in s
assert '0600,"sweet-pcm"' in s
prefix=r'''
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "sweet_pcm_ring.h"
#ifndef __unused
#define __unused __attribute__((unused))
#endif
#define FREAD 1
#define FWRITE 2
#define minor(d) ((unsigned)(d))
#define MIN(a,b) ((a)<(b)?(a):(b))
typedef unsigned dev_t;
typedef long probe_ssize_t;
typedef char *caddr_t;
typedef unsigned long u_long;
struct proc {int pid;};
struct uio {uint8_t *data;probe_ssize_t resid;int reading;};
static int root_cred=1,pcm_ring_lock,pcm_reader;
static struct proc current={11};
static struct sweet_pcm_ring pcm_ring;
struct sweet_audio_volume {uint32_t below,muted;};
static struct sweet_audio_volume pcm_volume;
#define SWEET_AUDIO_SET_VOLUME 101u
static int kauth_cred_get(void){return root_cred;}
static int kauth_cred_issuser(int cred){return cred;}
static int proc_pid(struct proc *p){return p->pid;}
static struct proc *current_proc(void){return &current;}
static void lck_mtx_lock(int *p){(void)p;}
static void lck_mtx_unlock(int *p){(void)p;}
static probe_ssize_t uio_resid(struct uio *u){return u->resid;}
static int uiomove(caddr_t b,int n,struct uio *u){
 assert(n>=0 && n<=u->resid);
 if(u->reading)memcpy(u->data,b,(size_t)n);else memcpy(b,u->data,(size_t)n);
 u->data+=n;u->resid-=n;return 0;
}
'''
tests=r'''
int main(void){
 assert(!pcm_dev_open(0,FWRITE,0,&current)); /* persistent HAL writer */
 assert(pcm_dev_open(0,FREAD,0,&current)==ENXIO);
 assert(pcm_dev_open(1,FWRITE,0,&current)==ENXIO);
 root_cred=0;assert(pcm_dev_open(1,FREAD,0,&current)==EPERM);root_cred=1;
 assert(!pcm_dev_open(1,FREAD,0,&current));
 struct proc other={12};assert(pcm_dev_open(1,FREAD,0,&other)==EBUSY);
 uint8_t data[]={1,2,3,4,5,6,7,8},out[8]={0};struct uio w={data,8,0};
 assert(!pcm_dev_write(0,&w,0));assert(pcm_ring.count==2);
 assert(!pcm_dev_close(0,FWRITE,0,&other));assert(pcm_reader==11 && pcm_ring.count==2);
 struct uio r={out,8,1};assert(!pcm_dev_read(1,&r,0));assert(!memcmp(data,out,8));
 current=other;r=(struct uio){out,8,1};assert(pcm_dev_read(1,&r,0)==EPERM);
 assert(pcm_dev_read(0,&r,0)==ENXIO);assert(pcm_dev_write(1,&w,0)==ENXIO);
 struct sweet_audio_volume v={0,1};assert(!pcm_dev_ioctl(0,SWEET_AUDIO_SET_VOLUME,(caddr_t)&v,0,&current));
 assert(pcm_dev_ioctl(1,SWEET_AUDIO_SET_VOLUME,(caddr_t)&v,0,&current)==ENOTTY);
 /* VFS last close on reader vnode now fires independently of HAL writers. */
 assert(!pcm_dev_close(1,FREAD,0,&current));assert(!pcm_reader);
 assert(!pcm_dev_open(1,FREAD,0,&other));assert(pcm_reader==12);
 assert(!pcm_dev_close(1,FREAD,0,&other));assert(!pcm_reader);
 puts("PASS: separate PCM endpoints, root-only/exclusive reader, owner checks, data, mute and reopen with HAL writer alive");
}
'''
with tempfile.TemporaryDirectory() as d:
 d=Path(d);(d/'test.c').write_text(prefix+functions+tests)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-I'+str(root/'userspace/adsp'),str(d/'test.c'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
