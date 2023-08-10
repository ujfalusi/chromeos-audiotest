// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEENSY_LATENCY_TEST_INCLUDE_COMMON_H
#define TEENSY_LATENCY_TEST_INCLUDE_COMMON_H

#include <alsa/asoundlib.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define PLAYBACK_SILENT_COUNT 10
#define PLAYBACK_COUNT 50

static double phase = M_PI / 2;
static unsigned rate = 48000;
static unsigned channels = 2;
static snd_pcm_uframes_t buffer_frames = 1024;
static snd_pcm_uframes_t period_size = 512;
static snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;

struct dolphin {
  int serial_fd;
};

static void generate_sine(const snd_pcm_channel_area_t* areas,
                          snd_pcm_uframes_t offset,
                          int count,
                          double* _phase) {
  static double max_phase = 2. * M_PI;
  double phase = *_phase;
  double step = max_phase * 1000 / (double)rate;
  unsigned char* samples[channels];
  int steps[channels];
  unsigned int chn;
  int format_bits = snd_pcm_format_width(format);
  unsigned int maxval = (1 << (format_bits - 1)) - 1;
  int bps = format_bits / 8; /* bytes per sample */
  int phys_bps = snd_pcm_format_physical_width(format) / 8;
  int big_endian = snd_pcm_format_big_endian(format) == 1;
  int to_unsigned = snd_pcm_format_unsigned(format) == 1;
  int is_float =
      (format == SND_PCM_FORMAT_FLOAT_LE || format == SND_PCM_FORMAT_FLOAT_BE);

  /* Verify and prepare the contents of areas */
  for (chn = 0; chn < channels; chn++) {
    if ((areas[chn].first % 8) != 0) {
      fprintf(stderr, "areas[%i].first == %i, aborting...\n", chn,
              areas[chn].first);
      exit(EXIT_FAILURE);
    }
    if ((areas[chn].step % 16) != 0) {
      fprintf(stderr, "areas[%i].step == %i, aborting...\n", chn,
              areas[chn].step);
      exit(EXIT_FAILURE);
    }
    steps[chn] = areas[chn].step / 8;
    samples[chn] = ((unsigned char*)areas[chn].addr) + (areas[chn].first / 8) +
                   offset * steps[chn];
  }

  /* Fill the channel areas */
  while (count-- > 0) {
    union {
      float f;
      int i;
    } fval;
    int res, i;
    if (is_float) {
      fval.f = sin(phase) * maxval;
      res = fval.i;
    } else {
      res = sin(phase) * maxval;
    }
    if (to_unsigned) {
      res ^= 1U << (format_bits - 1);
    }
    for (chn = 0; chn < channels; chn++) {
      /* Generate data in native endian format */
      if (big_endian) {
        for (i = 0; i < bps; i++) {
          *(samples[chn] + phys_bps - 1 - i) = (res >> i * 8) & 0xff;
        }
      } else {
        for (i = 0; i < bps; i++) {
          *(samples[chn] + i) = (res >> i * 8) & 0xff;
        }
      }
      samples[chn] += steps[chn];
    }
    phase += step;
    if (phase >= max_phase) {
      phase -= max_phase;
    }
  }
  *_phase = phase;
}

void set_format(const char* format_str) {
  format = snd_pcm_format_value(format_str);
  if (format == SND_PCM_FORMAT_UNKNOWN) {
    fprintf(stderr, "unknown format: %s\n", format_str);
    exit(EXIT_FAILURE);
  }
}

#endif  // TEENSY_LATENCY_TEST_INCLUDE_COMMON_H
