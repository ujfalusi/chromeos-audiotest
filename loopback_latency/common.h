// Copyright 2022 The ChromiumOS Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOOPBACK_LATENCY_COMMON_H_
#define LOOPBACK_LATENCY_COMMON_H_

#define CAPTURE_MORE_COUNT      50
#define PLAYBACK_COUNT          50
#define PLAYBACK_SILENT_COUNT   50
#define PLAYBACK_TIMEOUT_COUNT 100

#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>

extern double phase;
extern pthread_mutex_t latency_test_mutex;
extern pthread_cond_t sine_start;
extern int terminate_playback;
extern int terminate_capture;
extern int sine_started;

extern int capture_count;
extern int playback_count;

void generate_sine(const snd_pcm_channel_area_t *areas,
                   snd_pcm_uframes_t offset, int count,
                   double *_phase);

/* Looks for the first sample in buffer whose absolute value exceeds
 * noise_threshold. Returns the index of found sample in frames, -1
 * if not found. */
int check_for_noise(short *buf, unsigned len, unsigned channels);

unsigned long subtract_timevals(const struct timeval *end,
                                const struct timeval *beg);

#endif  // LOOPBACK_LATENCY_COMMON_H_
