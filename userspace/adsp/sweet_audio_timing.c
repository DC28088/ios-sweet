/* SPDX-License-Identifier: MIT */
/* Bounded timer probe. Opens no device and changes only its own thread QoS. */
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <sys/qos.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
static mach_timebase_info_data_t scale;
static void measure(const char *name,int absolute)
{
 uint64_t sum=0,min=UINT64_MAX,max=0;unsigned late=0,errors=0;
 const uint64_t ticks=(1000000ull*scale.denom+scale.numer-1)/scale.numer;
 for(unsigned i=0;i<64;i++){
  uint64_t begin=mach_absolute_time();
  if(absolute){if(mach_wait_until(begin+ticks)!=KERN_SUCCESS)++errors;}
  else if(usleep(1000))++errors;
  uint64_t ns=(mach_absolute_time()-begin)*scale.numer/scale.denom;
  sum+=ns;if(ns<min)min=ns;if(ns>max)max=ns;late+=ns>2000000;
 }
 printf("timer %s: requested_us=1000 samples=64 min_us=%llu avg_us=%llu max_us=%llu over2ms=%u errors=%u\n",
  name,(unsigned long long)(min/1000),(unsigned long long)(sum/64/1000),
  (unsigned long long)(max/1000),late,errors);
}
int main(void)
{
 setvbuf(stdout,NULL,_IONBF,0);
 if(mach_timebase_info(&scale)!=KERN_SUCCESS || !scale.numer || !scale.denom)return 1;
 measure("default-usleep",0);measure("default-mach-wait",1);
 int rc=pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE,0);
 printf("timer own-thread interactive QoS result=%d\n",rc);
 if(!rc){measure("interactive-usleep",0);measure("interactive-mach-wait",1);}
 return 0;
}
