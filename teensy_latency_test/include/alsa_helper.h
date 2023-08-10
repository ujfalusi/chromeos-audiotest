// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEENSY_LATENCY_TEST_INCLUDE_ALSA_HELPER_H_
#define TEENSY_LATENCY_TEST_INCLUDE_ALSA_HELPER_H_

#include <alsa/asoundlib.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "common.h"

static snd_pcm_sframes_t playback_delay_frames;

static void config_pcm(snd_pcm_t* handle,
                       unsigned int rate,
                       unsigned int channels,
                       snd_pcm_format_t format,
                       snd_pcm_uframes_t* buffer_size,
                       snd_pcm_uframes_t* period_size) {
  int err;
  snd_pcm_hw_params_t* hw_params;

  if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
    fprintf(stderr, "cannot allocate hardware parameter structure (%s)\n",
            snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_hw_params_any(handle, hw_params)) < 0) {
    fprintf(stderr, "cannot initialize hardware parameter structure (%s)\n",
            snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_hw_params_set_access(handle, hw_params,
                                          SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    fprintf(stderr, "cannot set access type (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_hw_params_set_format(handle, hw_params, format)) < 0) {
    fprintf(stderr, "cannot set sample format (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_hw_params_set_rate_near(handle, hw_params, &rate, 0)) <
      0) {
    fprintf(stderr, "cannot set sample rate (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_hw_params_set_channels(handle, hw_params, 2)) < 0) {
    fprintf(stderr, "cannot set channel count (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_hw_params_set_buffer_size_near(handle, hw_params,
                                                    buffer_size)) < 0) {
    fprintf(stderr, "cannot set buffer size (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_hw_params_set_period_size_near(handle, hw_params,
                                                    period_size, 0)) < 0) {
    fprintf(stderr, "cannot set period size (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_hw_params(handle, hw_params)) < 0) {
    fprintf(stderr, "cannot set parameters (%s)\n", snd_strerror(err));
    exit(1);
  }

  snd_pcm_hw_params_free(hw_params);

  if ((err = snd_pcm_prepare(handle)) < 0) {
    fprintf(stderr, "cannot prepare audio interface for use (%s)\n",
            snd_strerror(err));
    exit(1);
  }
}

static void* alsa_play(void* arg, struct dolphin* d) {
  snd_pcm_t* handle = (snd_pcm_t*)arg;
  short* play_buf;
  snd_pcm_channel_area_t* areas;
  unsigned int chn, num_buffers;
  int err;

  play_buf = calloc(buffer_frames * channels, sizeof(play_buf[0]));
  areas = calloc(channels, sizeof(snd_pcm_channel_area_t));

  for (chn = 0; chn < channels; chn++) {
    areas[chn].addr = play_buf;
    areas[chn].first = chn * snd_pcm_format_physical_width(format);
    areas[chn].step = channels * snd_pcm_format_physical_width(format);
  }

  num_buffers = 0;
  int avail_frames;
  avail_frames = snd_pcm_avail(handle);
  memset(play_buf, 0, buffer_frames * channels);
  if ((err = snd_pcm_writei(handle, play_buf, avail_frames)) < 0) {
    fprintf(stderr, "failed to fill zeroes (%s)\n", snd_strerror(err));
  }

  /* Play a sine wave and look for it on capture thread.
   * This will fail for latency > 500mS. */
  while (num_buffers < PLAYBACK_COUNT) {
    avail_frames = snd_pcm_avail(handle);
    while (avail_frames >= (int)period_size) {
      generate_sine(areas, 0, period_size, &phase);
      if (num_buffers < 1) {
        int res;
        const uint8_t msg = 'c';
        uint8_t reply_msg;
        res = write(d->serial_fd, &msg, 1);
        if (res != 1) {
          fprintf(stderr, "Fail to toggle audio\n");
        }
        res = read(d->serial_fd, &reply_msg, 1);
        if (res != 1) {
          fprintf(stderr, "Fail to start audio\n");
        }
        printf("play\n");

        snd_pcm_delay(handle, &playback_delay_frames);
      }
      if ((err = snd_pcm_writei(handle, play_buf, period_size)) !=
          period_size) {
        fprintf(stderr, "write to audio interface failed (%s)\n",
                snd_strerror(err));
      }
      num_buffers++;
      avail_frames = snd_pcm_avail(handle);
      printf("num_buffers: %d, avail_frames: %d\n", num_buffers, avail_frames);
    }
  }
  unsigned long latency_us = playback_delay_frames * 1000000 / rate;
  printf("Reported latency: %luus\n", latency_us);

  free(play_buf);
  free(areas);
  return 0;
}

snd_pcm_t* opendev(char* play_dev) {
  snd_pcm_t* handle;
  int err;
  if ((err = snd_pcm_open(&handle, play_dev, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    fprintf(stderr, "cannot open audio device %s (%s)\n", play_dev,
            snd_strerror(err));
    exit(1);
  }
  return handle;
}

void play(snd_pcm_t* handle, struct dolphin* d) {
  config_pcm(handle, rate, channels, format, &buffer_frames, &period_size);
  printf("%lu %ld\n", buffer_frames, period_size);
  alsa_play(handle, d);
  snd_pcm_close(handle);
}

#endif  // TEENSY_LATENCY_TEST_INCLUDE_ALSA_HELPER_H_
