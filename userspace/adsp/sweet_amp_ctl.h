/* SPDX-License-Identifier: MIT */
#ifndef SWEET_AMP_CTL_H
#define SWEET_AMP_CTL_H
#ifdef KERNEL
#include <sys/types.h>
#include <sys/ioccom.h>
#else
#include <stdint.h>
#include <sys/ioctl.h>
#endif
#define SWEET_AMP_DEVICE "/dev/sweet-amp"
#define SWEET_AMP_PROFILE_MAX 64u
struct sweet_amp_profile {uint32_t channel,count;uint8_t records[SWEET_AMP_PROFILE_MAX*4];};
struct sweet_amp_start {uint32_t channels,attenuation_eighth_db;};
struct sweet_amp_status {uint32_t prepared,active,expired,bus_dead;uint32_t sysst[2];};
#define SWEET_AMP_LOAD _IOW('A',1,struct sweet_amp_profile)
#define SWEET_AMP_PREPARE _IOW('A',2,uint32_t)
#define SWEET_AMP_HEARTBEAT _IO('A',3)
#define SWEET_AMP_START _IOW('A',4,struct sweet_amp_start)
#define SWEET_AMP_STOP _IO('A',5)
#define SWEET_AMP_STATUS _IOR('A',6,struct sweet_amp_status)
#endif
