// Copyright 2022 The ChromiumOS Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOOPBACK_LATENCY_ARGS_H_
#define LOOPBACK_LATENCY_ARGS_H_

#define PIN_DEVICE_UNSET -1

#include <alsa/asoundlib.h>

extern unsigned channels;
extern snd_pcm_format_t format;
extern snd_pcm_uframes_t buffer_frames;
extern snd_pcm_uframes_t period_size;
extern snd_pcm_uframes_t start_threshold;
extern unsigned rate;
extern int noise_threshold;
extern int loop;
extern int cold;
extern int pin_capture_device;

#endif  // LOOPBACK_LATENCY_ARGS_H_
