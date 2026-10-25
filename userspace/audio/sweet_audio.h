/*
 * The plug-in's side of the sweet kernel's audio devices (userspace/audio/sweet_audio_plugin.c).
 * Adapted from HackiOS userland/src/redwood_audio.h (MIT, Copyright (c) 2026 OOSEX GANG).
 *
 *   /dev/sweet-audio   the speaker's stereo frames, in the format below; write() never blocks and
 *                      what the kernel cannot take is dropped. Not in the kernel yet: the plug-in
 *                      counts the frames as refused.
 *   /dev/sweet-haptic  the actuator's mono samples, same rate (bsd/dev/sweet_haptic.c, sweethaptic=3)
 *   /dev/sweet-mic     the microphone. Not in the kernel yet: the input is silence.
 */
#ifndef _SWEET_AUDIO_H_
#define _SWEET_AUDIO_H_

#include <stdint.h>
#include <sys/ioccom.h>

#define SWEET_AUDIO_DEVICE      "/dev/sweet-audio"

/* The one format: 16-bit signed little-endian samples, left then right. */
#define SWEET_AUDIO_RATE        48000
#define SWEET_AUDIO_CHANNELS    2
#define SWEET_AUDIO_FRAME       4       /* bytes */

/* ioctl(SWEET_AUDIO_SET_VOLUME): eighths of a dB under the loudest, and whether muted */
struct sweet_audio_volume {
	uint32_t below;
	uint32_t muted;
};
#define SWEET_AUDIO_SET_VOLUME  _IOW('r', 1, struct sweet_audio_volume)

#endif
