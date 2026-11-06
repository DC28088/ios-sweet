/* SPDX-License-Identifier: MIT */
#ifndef SWEET_AW2032_H
#define SWEET_AW2032_H
#include <stddef.h>
#include <stdint.h>
/* ACF is supplied privately from the device's vendor partition, never embedded.
 * Only per-device Music register profiles are supported; no DSP/OTP programming. */
struct sweet_aw_profile {const uint8_t *records;size_t count;};
int sweet_aw_profile_select(const uint8_t *acf,size_t bytes,unsigned channel,struct sweet_aw_profile *out);
struct sweet_aw_io {
 void *ctx;
 int (*read)(void *,uint8_t,uint16_t *);
 int (*write)(void *,uint8_t,uint16_t);
 int (*reset)(void *,int asserted);
 void (*sleep_ms)(void *,unsigned);
};
struct sweet_aw2032 {
 struct sweet_aw_io io;
 unsigned prepared,active,verified;
 uint16_t profile_volume,last_status;
};
/* Hardware reset, chip-ID check, profile under mute/powerdown, per-chip efuse
 * calibration, 48kHz/16bit format. Any error asserts the hardware reset. */
int sweet_aw2032_prepare(struct sweet_aw2032 *,const struct sweet_aw_io *,const struct sweet_aw_profile *);
/* Diagnostic limit: at least 24dB digital attenuation. Caller must first have
 * confirmed PCM completions, then poll faults and stop within a bounded trial. */
int sweet_aw2032_start(struct sweet_aw2032 *,unsigned attenuation_eighth_db);
int sweet_aw2032_poll(struct sweet_aw2032 *);
void sweet_aw2032_stop(struct sweet_aw2032 *);
#endif
