// Copyright 2022 The ChromiumOS Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <alsa/asoundlib.h>
#include <math.h>

#include "args.h"

unsigned channels = 2;
snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
snd_pcm_uframes_t buffer_frames = 480;
snd_pcm_uframes_t period_size = 240;
snd_pcm_uframes_t start_threshold = 0;
unsigned rate = 48000;
int noise_threshold = 0x4000;
int loop;
int cold;
int pin_capture_device = PIN_DEVICE_UNSET;
