// Copyright 2022 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <alsa/asoundlib.h>
#include <pthread.h>
#include <sys/time.h>

#include "args.h"
#include "common.h"

static snd_pcm_sframes_t playback_delay_frames;
static struct timeval sine_start_tv;

static void config_pcm_hw_params(snd_pcm_t* handle,
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

  if ((err = snd_pcm_hw_params_set_channels(handle, hw_params, channels)) < 0) {
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
    fprintf(stderr, "cannot set hardware parameters (%s)\n", snd_strerror(err));
    exit(1);
  }

  snd_pcm_hw_params_free(hw_params);

  if ((err = snd_pcm_prepare(handle)) < 0) {
    fprintf(stderr, "cannot prepare audio interface for use (%s)\n",
            snd_strerror(err));
    exit(1);
  }
}

static void config_pcm_sw_params(snd_pcm_t* handle,
                                 snd_pcm_uframes_t start_threshold) {
  int err;
  snd_pcm_sw_params_t* sw_params;

  if ((err = snd_pcm_sw_params_malloc(&sw_params)) < 0) {
    fprintf(stderr, "cannot allocate software parameter structure (%s)\n",
            snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_sw_params_current(handle, sw_params)) < 0) {
    fprintf(stderr, "cannot get current sw parameter structure (%s)\n",
            snd_strerror(err));
    exit(1);
  }

  if (start_threshold > 0) {
    if ((err = snd_pcm_sw_params_set_start_threshold(handle, sw_params,
                                                     start_threshold)) < 0) {
      fprintf(stderr, "cannot set start threshold (%s)\n", snd_strerror(err));
      exit(1);
    }
  }

  if (snd_pcm_sw_params(handle, sw_params) < 0) {
    fprintf(stderr, "cannot set software parameters (%s)\n", snd_strerror(err));
    exit(1);
  }

  snd_pcm_sw_params_free(sw_params);
}

static int capture_some(snd_pcm_t* pcm,
                        short* buf,
                        unsigned len,
                        snd_pcm_sframes_t* cap_delay_frames) {
  snd_pcm_sframes_t frames = snd_pcm_avail(pcm);
  int err;

  if (frames > 0) {
    frames = frames > len ? len : frames;

    snd_pcm_delay(pcm, cap_delay_frames);
    if ((err = snd_pcm_readi(pcm, buf, frames)) != frames) {
      fprintf(stderr, "read from audio interface failed (%s)\n",
              snd_strerror(err));
      exit(1);
    }
  }

  return (int)frames;
}

static void* alsa_play(void* arg) {
  snd_pcm_t* handle = (snd_pcm_t*)arg;
  short* play_buf;
  snd_pcm_channel_area_t* areas;
  unsigned int chn, num_buffers;
  int err;

  play_buf = calloc(g_buffer_frames * g_channels, sizeof(play_buf[0]));
  areas = calloc(g_channels, sizeof(snd_pcm_channel_area_t));

  for (chn = 0; chn < g_channels; chn++) {
    areas[chn].addr = play_buf;
    areas[chn].first = chn * snd_pcm_format_physical_width(g_format);
    areas[chn].step = g_channels * snd_pcm_format_physical_width(g_format);
  }

  for (num_buffers = 0; num_buffers < PLAYBACK_SILENT_COUNT; num_buffers++) {
    if ((err = snd_pcm_writei(handle, play_buf, g_period_size)) !=
        g_period_size) {
      fprintf(stderr,
              "write %dth silent block to audio interface \
                    failed (%s)\n",
              num_buffers, snd_strerror(err));
      exit(1);
    }
  }

  generate_sine(areas, 0, g_period_size, &g_phase);
  snd_pcm_delay(handle, &playback_delay_frames);
  gettimeofday(&sine_start_tv, NULL);

  num_buffers = 0;
  int avail_frames;

  /* Play a sine wave and look for it on capture thread.
   * This will fail for latency > 500mS. */
  while (!g_terminate_playback && num_buffers < PLAYBACK_COUNT) {
    avail_frames = snd_pcm_avail(handle);
    if (avail_frames >= g_period_size) {
      pthread_mutex_lock(&g_latency_test_mutex);
      if (!g_sine_started) {
        g_sine_started = 1;
        pthread_cond_signal(&g_sine_start);
      }
      pthread_mutex_unlock(&g_latency_test_mutex);
      if ((err = snd_pcm_writei(handle, play_buf, g_period_size)) !=
          g_period_size) {
        fprintf(stderr, "write to audio interface failed (%s)\n",
                snd_strerror(err));
      }
      num_buffers++;
    }
  }
  g_terminate_playback = 1;

  if (num_buffers == PLAYBACK_COUNT)
    fprintf(stdout, "Audio not detected.\n");

  free(play_buf);
  free(areas);
  return 0;
}

static void* alsa_capture(void* arg) {
  int err;
  short* cap_buf;
  snd_pcm_t* capture_handle = (snd_pcm_t*)arg;
  snd_pcm_sframes_t cap_delay_frames;
  int num_cap, noise_delay_frames;

  cap_buf = calloc(g_buffer_frames * g_channels, sizeof(cap_buf[0]));

  pthread_mutex_lock(&g_latency_test_mutex);
  while (!g_sine_started) {
    pthread_cond_wait(&g_sine_start, &g_latency_test_mutex);
  }
  pthread_mutex_unlock(&g_latency_test_mutex);

  /* Begin capture. */
  if ((err = snd_pcm_start(capture_handle)) < 0) {
    fprintf(stderr, "cannot start audio interface for use (%s)\n",
            snd_strerror(err));
    exit(1);
  }

  while (!g_terminate_capture) {
    num_cap = capture_some(capture_handle, cap_buf, g_buffer_frames,
                           &cap_delay_frames);

    if (num_cap > 0 && (noise_delay_frames = check_for_noise(
                            cap_buf, num_cap, g_channels)) >= 0) {
      struct timeval cap_time;
      unsigned long latency_us;
      gettimeofday(&cap_time, NULL);

      fprintf(stderr, "Found audio\n");
      fprintf(stderr, "Played at %llu %llu, %ld delay\n",
              (unsigned long long)sine_start_tv.tv_sec,
              (unsigned long long)sine_start_tv.tv_usec, playback_delay_frames);
      fprintf(stderr, "Capture at %llu %llu, %ld delay sample %d\n",
              (unsigned long long)cap_time.tv_sec,
              (unsigned long long)cap_time.tv_usec, cap_delay_frames,
              noise_delay_frames);

      latency_us = subtract_timevals(&cap_time, &sine_start_tv);
      fprintf(stdout, "Measured Latency: %lu uS\n", latency_us);

      latency_us =
          (playback_delay_frames + cap_delay_frames - noise_delay_frames) *
          1000000 / g_rate;
      fprintf(stdout, "Reported Latency: %lu uS\n", latency_us);

      // Noise captured, terminate both threads.
      g_terminate_playback = 1;
      g_terminate_capture = 1;
    } else {
      // Capture some more buffers after playback thread has terminated.
      if (g_terminate_playback && g_capture_count++ < CAPTURE_MORE_COUNT)
        g_terminate_capture = 1;
    }
  }

  free(cap_buf);
  return 0;
}

void alsa_test_latency(char* play_dev, char* cap_dev) {
  int err;
  snd_pcm_t* playback_handle;
  snd_pcm_t* capture_handle;

  pthread_t capture_thread;
  pthread_t playback_thread;

  if ((err = snd_pcm_open(&playback_handle, play_dev, SND_PCM_STREAM_PLAYBACK,
                          0)) < 0) {
    fprintf(stderr, "cannot open audio device %s (%s)\n", play_dev,
            snd_strerror(err));
    exit(1);
  }
  config_pcm_hw_params(playback_handle, g_rate, g_channels, g_format,
                       &g_buffer_frames, &g_period_size);
  config_pcm_sw_params(playback_handle, g_start_threshold);

  if ((err = snd_pcm_open(&capture_handle, cap_dev, SND_PCM_STREAM_CAPTURE,
                          0)) < 0) {
    fprintf(stderr, "cannot open audio device %s (%s)\n", cap_dev,
            snd_strerror(err));
    exit(1);
  }
  config_pcm_hw_params(capture_handle, g_rate, g_channels, g_format,
                       &g_buffer_frames, &g_period_size);

  pthread_mutex_init(&g_latency_test_mutex, NULL);
  pthread_cond_init(&g_sine_start, NULL);

  pthread_create(&playback_thread, NULL, alsa_play, playback_handle);
  pthread_create(&capture_thread, NULL, alsa_capture, capture_handle);

  pthread_join(capture_thread, NULL);
  pthread_join(playback_thread, NULL);

  snd_pcm_close(playback_handle);
  snd_pcm_close(capture_handle);
}
