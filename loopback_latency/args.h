// Copyright 2022 The ChromiumOS Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOOPBACK_LATENCY_ARGS_H_
#define LOOPBACK_LATENCY_ARGS_H_

#define PIN_DEVICE_UNSET -1

#include <alsa/asoundlib.h>

extern unsigned g_channels;
extern snd_pcm_format_t g_format;
extern snd_pcm_uframes_t g_buffer_frames;
extern snd_pcm_uframes_t g_period_size;
extern snd_pcm_uframes_t g_start_threshold;
extern unsigned g_rate;
extern int g_noise_threshold;
extern int g_loop;
extern int g_cold;
extern int g_pin_capture_device;

#endif  // LOOPBACK_LATENCY_ARGS_H_
