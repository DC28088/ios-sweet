/* SPDX-License-Identifier: MIT */
/* Read-only HAL level probe. Does not open pmem, DSP or amplifier devices. */
#include "sweet_pcm_levels.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
static uint64_t millis(void)
{
 struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
 return (uint64_t)t.tv_sec*1000+(unsigned)t.tv_nsec/1000000;
}
int main(int argc,char **argv)
{
 unsigned seconds=30;
 if(argc>2)return 2;
 if(argc==2){char *end;unsigned long v=strtoul(argv[1],&end,10);if(!*argv[1]||*end||v<1||v>60)return 2;seconds=(unsigned)v;}
 int fd=open("/dev/sweet-audio",O_RDONLY|O_NONBLOCK|O_CLOEXEC);
 if(fd<0){perror("PCM reader");return 1;}
 setvbuf(stdout,NULL,_IONBF,0);
 printf("PCM level probe: %u seconds; aggregate levels only, no audio recorded\n",seconds);
 uint64_t start=millis(),next=start+1000;
 struct sweet_pcm_levels levels={0};
 while(millis()-start<(uint64_t)seconds*1000){
  uint8_t bytes[1920];ssize_t n=read(fd,bytes,sizeof bytes);
  if(n>0){if(n&3){close(fd);return 1;}sweet_pcm_levels_add(&levels,bytes,(size_t)n);}
  else if(n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR){perror("PCM read");close(fd);return 1;}
  else usleep(2000);
  uint64_t now=millis();
  if(now>=next){
   printf("PCM levels: elapsed_ms=%llu frames=%llu nonzero_samples=%llu peak=%u mean_square=%llu\n",
    (unsigned long long)(now-start),(unsigned long long)levels.frames,
    (unsigned long long)levels.nonzero_samples,levels.peak,
    (unsigned long long)(levels.frames?levels.squares/(levels.frames*2):0));
   levels=(struct sweet_pcm_levels){0};next=now+1000;
  }
 }
 close(fd);return 0;
}
