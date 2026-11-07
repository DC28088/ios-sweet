/* SPDX-License-Identifier: MIT */
/* Briefly compare timer wakes with coalescing off, restoring the saved value.
 * No device opens, process policy changes, DSP or amplifier operations. */
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int original,dirty;
static volatile sig_atomic_t stopping;
static mach_timebase_info_data_t scale;
static const char *key="kern.timer.coalescing_enabled";
static void stop_test(int sig){(void)sig;stopping=1;}
static void restore(void)
{
 if(!dirty)return;
 int rc=sysctlbyname(key,NULL,NULL,&original,sizeof original);
 int value=-1;size_t n=sizeof value;
 int read_rc=sysctlbyname(key,&value,&n,NULL,0);
 printf("coalescing restore: result=%d read_result=%d value=%d expected=%d\n",rc,read_rc,value,original);
 dirty=rc || read_rc || value!=original;
}
static void measure(const char *label)
{
 uint64_t total=0,max=0;unsigned count=0,errors=0;
 uint64_t step=(1000000ull*scale.denom+scale.numer-1)/scale.numer;
 while(count<64 && !stopping){
  uint64_t begin=mach_absolute_time();
  if(mach_wait_until(begin+step)!=KERN_SUCCESS)++errors;
  uint64_t ns=(mach_absolute_time()-begin)*scale.numer/scale.denom;
  total+=ns;if(ns>max)max=ns;++count;
 }
 printf("coalescing %s: samples=%u requested_us=1000 avg_us=%llu max_us=%llu errors=%u\n",label,count,
  (unsigned long long)(count?total/count/1000:0),(unsigned long long)(max/1000),errors);
}
/* Hold coalescing off for a bounded listening window; always restore. */
static int hold_off(unsigned seconds)
{
 int zero=0;
 if(sysctlbyname(key,NULL,NULL,&zero,sizeof zero)){printf("coalescing: cannot disable; no setting changed\n");return 1;}
 dirty=1;printf("coalescing off: seconds=%u\n",seconds);
 unsigned left=seconds;
 while(left && !stopping){unsigned n=sleep(left);if(n>=left)break;left=n;}
 restore();
 printf("coalescing off finished: restored=%d\n",!dirty);
 return dirty?1:0;
}
int main(int argc,char **argv)
{
 setvbuf(stdout,NULL,_IONBF,0);
 if(argc==2 && !strcmp(argv[1],"--restore")){int value=1;return sysctlbyname(key,NULL,NULL,&value,sizeof value)?1:0;}
 unsigned off_seconds=0;
 if(argc==3 && !strcmp(argv[1],"--off")){
  char *end=NULL;unsigned long v=strtoul(argv[2],&end,10);
  if(!*argv[2] || *end || v<1 || v>300){printf("coalescing: --off seconds must be1..300\n");return 2;}
  off_seconds=(unsigned)v;
 }else if(argc!=1)return 2;
 if(mach_timebase_info(&scale)!=KERN_SUCCESS || !scale.numer || !scale.denom)return 1;
 size_t n=sizeof original;
 if(sysctlbyname(key,&original,&n,NULL,0) || n!=sizeof original || original!=1){
  printf("coalescing: cannot establish enabled baseline; no setting changed\n");return 1;
 }
 signal(SIGTERM,stop_test);signal(SIGINT,stop_test);signal(SIGHUP,stop_test);atexit(restore);
 if(off_seconds)return hold_off(off_seconds);
 measure("enabled");if(stopping)return 1;
 int zero=0;
 if(sysctlbyname(key,NULL,NULL,&zero,sizeof zero)){printf("coalescing: cannot disable; no setting changed\n");return 1;}
 dirty=1;measure("disabled");restore();return dirty?1:0;
}
