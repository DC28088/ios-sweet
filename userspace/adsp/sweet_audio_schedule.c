/* SPDX-License-Identifier: MIT */
/* Bounded scheduling experiment. No PCM, DSP, MMIO or amplifier operations. */
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
static mach_timebase_info_data_t scale;
static volatile sig_atomic_t stopping;
static void stop_test(int signal_number){(void)signal_number;stopping=1;}
static uint32_t ticks(unsigned ns)
{
 uint64_t value=((uint64_t)ns*scale.denom+scale.numer-1)/scale.numer;
 return value>UINT32_MAX?0:(uint32_t)value;
}
static int save_standard(thread_t thread,thread_extended_policy_data_t *saved)
{
 boolean_t def=FALSE;mach_msg_type_number_t count=THREAD_TIME_CONSTRAINT_POLICY_COUNT;
 thread_time_constraint_policy_data_t prior;
 kern_return_t rc=thread_policy_get(thread,THREAD_TIME_CONSTRAINT_POLICY,(thread_policy_t)&prior,&count,&def);
 if(rc!=KERN_SUCCESS || !def){printf("schedule: refuses unavailable/already-realtime thread policy (%d)\n",rc);return -1;}
 def=FALSE;count=THREAD_EXTENDED_POLICY_COUNT;
 rc=thread_policy_get(thread,THREAD_EXTENDED_POLICY,(thread_policy_t)saved,&count,&def);
 return rc==KERN_SUCCESS && !def?0:-1;
}
static kern_return_t realtime(thread_t thread)
{
 thread_time_constraint_policy_data_t p={ticks(10000000),ticks(2000000),ticks(5000000),TRUE};
 if(!p.period || !p.computation || !p.constraint)return KERN_INVALID_ARGUMENT;
 return thread_policy_set(thread,THREAD_TIME_CONSTRAINT_POLICY,(thread_policy_t)&p,THREAD_TIME_CONSTRAINT_POLICY_COUNT);
}
static kern_return_t restore(thread_t thread,thread_extended_policy_data_t *saved)
{
 return thread_policy_set(thread,THREAD_EXTENDED_POLICY,(thread_policy_t)saved,THREAD_EXTENDED_POLICY_COUNT);
}
static unsigned measure(const char *name,int absolute)
{
 uint64_t sum=0,max=0;unsigned errors=0;
 for(unsigned i=0;i<64;i++){
  uint64_t begin=mach_absolute_time();
  if(absolute){if(mach_wait_until(begin+ticks(1000000))!=KERN_SUCCESS)++errors;}
  else if(usleep(1000))++errors;
  uint64_t ns=(mach_absolute_time()-begin)*scale.numer/scale.denom;
  sum+=ns;if(ns>max)max=ns;
 }
 unsigned average=(unsigned)(sum/64/1000);
 printf("schedule %s: requested_us=1000 avg_us=%u max_us=%llu errors=%u\n",name,average,(unsigned long long)(max/1000),errors);
 return errors?UINT_MAX:average;
}
int main(int argc,char **argv)
{
 setvbuf(stdout,NULL,_IONBF,0);
 if(argc!=1 && (argc!=3 || strcmp(argv[1],"--pid")))return 2;
 if(mach_timebase_info(&scale)!=KERN_SUCCESS || !scale.numer || !scale.denom)return 1;
 thread_t own=mach_thread_self();thread_extended_policy_data_t saved;
 if(save_standard(own,&saved))return 1;
 kern_return_t rc=realtime(own);printf("schedule self realtime result=%d; 2ms budget/10ms period, preemptible\n",rc);
 unsigned average=UINT_MAX;
 if(rc==KERN_SUCCESS){measure("realtime-usleep",0);average=measure("realtime-mach-wait",1);
  kern_return_t undo=restore(own,&saved);printf("schedule self restore result=%d\n",undo);if(undo!=KERN_SUCCESS)return 1;}
 mach_port_deallocate(mach_task_self(),own);
 if(argc==1)return 0;
 /* Do not touch a live process unless the independent self-test improves waits. */
 if(rc!=KERN_SUCCESS || average>3000){printf("schedule: live test skipped; self-test did not establish precise waits\n");return 1;}
 char *end;long value=strtol(argv[2],&end,10);
 if(!*argv[2] || *end || value<=1 || value>INT_MAX)return 2;
 task_t task=MACH_PORT_NULL;rc=task_for_pid(mach_task_self(),(int)value,&task);
 if(rc!=KERN_SUCCESS){printf("schedule task access result=%d; no target change\n",rc);return 1;}
 thread_act_array_t threads=NULL;mach_msg_type_number_t count=0;
 rc=task_threads(task,&threads,&count);
 if(rc!=KERN_SUCCESS){mach_port_deallocate(mach_task_self(),task);return 1;}
 int result=1;
 if(count!=1){printf("schedule: requires exactly one target thread; found %u, no change\n",count);goto cleanup;}
 if(save_standard(threads[0],&saved))goto cleanup;
 rc=realtime(threads[0]);printf("schedule live realtime result=%d; automatic restore after60s\n",rc);
 if(rc==KERN_SUCCESS){
  signal(SIGTERM,stop_test);signal(SIGINT,stop_test);
  uint64_t until=mach_absolute_time()+(uint64_t)ticks(10000000)*6000;
  while(!stopping && mach_absolute_time()<until)usleep(100000);
  rc=restore(threads[0],&saved);printf("schedule live restore result=%d\n",rc);result=rc==KERN_SUCCESS?0:1;
 }
cleanup:
 for(mach_msg_type_number_t i=0;i<count;i++)mach_port_deallocate(mach_task_self(),threads[i]);
 vm_deallocate(mach_task_self(),(vm_address_t)threads,(vm_size_t)count*sizeof(*threads));
 mach_port_deallocate(mach_task_self(),task);return result;
}
