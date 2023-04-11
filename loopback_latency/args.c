// Copyright 2022 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <alsa/asoundlib.h>
#include <math.h>

#include "args.h"

unsigned g_channels = 2;
snd_pcm_format_t g_format = SND_PCM_FORMAT_S16_LE;
snd_pcm_uframes_t g_buffer_frames = 480;
snd_pcm_uframes_t g_period_size = 240;
snd_pcm_uframes_t g_start_threshold = 0;
unsigned g_rate = 48000;
int g_noise_threshold = 0x4000;
int g_loop;
int g_cold;
int g_pin_capture_device = PIN_DEVICE_UNSET;
