// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEENSY_LATENCY_TEST_INCLUDE_CRAS_HELPER_H_
#define TEENSY_LATENCY_TEST_INCLUDE_CRAS_HELPER_H_

#include <alsa/asoundlib.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "common.h"
#include "cras_client.h"

static struct timeval* cras_play_time = NULL;
static struct dolphin* dolphin = NULL;
static int playback_count = 0;
static int sine_started = 0;
struct timespec last_playback_latency;

/* Callback for tone playback.  Playback latency will be passed
 * as arg and updated at the first sine tone right after silent.
 */
static int cras_play_tone(struct cras_client* client,
                          cras_stream_id_t stream_id,
                          uint8_t* samples,
                          size_t frames,
                          const struct timespec* sample_time,
                          void* arg) {
  snd_pcm_channel_area_t* areas;
  int chn;
  size_t sample_bytes;

  sample_bytes = snd_pcm_format_physical_width(format) / 8;

  areas = calloc(channels, sizeof(snd_pcm_channel_area_t));
  for (chn = 0; chn < channels; chn++) {
    areas[chn].addr = samples + chn * sample_bytes;
    areas[chn].first = 0;
    areas[chn].step = channels * snd_pcm_format_physical_width(format);
  }

  if (playback_count > PLAYBACK_COUNT) {
    /* for loop mode: to avoid underrun */
    memset(samples, 0, sample_bytes * frames * channels);
  } else if (playback_count < PLAYBACK_SILENT_COUNT) {
    memset(samples, 0, sample_bytes * frames * channels);
  } else {
    generate_sine(areas, 0, frames, &phase);
    if (!sine_started) {
      if (dolphin) {
        int res;
        const uint8_t msg = 'c';
        uint8_t reply_msg;
        res = write(dolphin->serial_fd, &msg, 1);
        if (res != 1) {
          fprintf(stderr, "Fail to toggle audio\n");
        }
        res = read(dolphin->serial_fd, &reply_msg, 1);
        if (res != 1) {
          fprintf(stderr, "Fail to start audio\n");
        }
        fprintf(stderr, "play\n");
      }
      /* Signal that sine tone started playing and update playback time
       * and latency at first played frame. */
      sine_started = 1;
      cras_client_calc_playback_latency(sample_time, (struct timespec*)arg);
      cras_play_time = (struct timeval*)malloc(sizeof(*cras_play_time));
      gettimeofday(cras_play_time, NULL);
    }
    cras_client_calc_playback_latency(sample_time, &last_playback_latency);
  }

  playback_count++;
  return frames;
}

static int stream_error(struct cras_client* client,
                        cras_stream_id_t stream_id,
                        int err,
                        void* arg) {
  fprintf(stderr, "Stream error %d\n", err);
  return 0;
}

/* Adds stream to cras client.  */
static int cras_add_output_stream(struct cras_client* client,
                                  struct cras_stream_params* params,
                                  struct timespec* user_data) {
  struct cras_audio_format* aud_format;
  cras_playback_cb_t aud_cb;
  cras_error_cb_t error_cb;
  size_t cb_threshold = buffer_frames;
  size_t min_cb_level = buffer_frames;
  int rc = 0;
  cras_stream_id_t stream_id = 0;

  aud_format = cras_audio_format_create(format, rate, channels);
  if (aud_format == NULL) {
    return -ENOMEM;
  }

  /* Create and start stream */
  aud_cb = cras_play_tone;
  error_cb = stream_error;
  params = cras_client_stream_params_create(
      CRAS_STREAM_OUTPUT, buffer_frames, cb_threshold, min_cb_level, 0, 0,
      user_data, aud_cb, error_cb, aud_format);
  if (params == NULL) {
    return -ENOMEM;
  }

  rc = cras_client_add_stream(client, &stream_id, params);
  if (rc < 0) {
    fprintf(stderr, "Add a stream fail.\n");
    return rc;
  }
  cras_audio_format_destroy(aud_format);
  return 0;
}

void cras_test_latency(struct dolphin* d) {
  int rc;
  struct cras_client* client = NULL;
  struct cras_stream_params* playback_params = NULL;

  struct timespec playback_latency;

  dolphin = d;

  rc = cras_client_create(&client);
  if (rc < 0) {
    fprintf(stderr, "Create client fail.\n");
    exit(1);
  }
  rc = cras_client_connect(client);
  if (rc < 0) {
    fprintf(stderr, "Connect to server fail.\n");
    cras_client_destroy(client);
    exit(1);
  }

  cras_client_run_thread(client);

  fprintf(stderr, "Create playback stream.\n");
  rc = cras_add_output_stream(client, playback_params, &playback_latency);
  if (rc < 0) {
    fprintf(stderr, "Fail to add playback stream.\n");
    exit(1);
  }
  struct timespec delay = {.tv_sec = 0, .tv_nsec = 500000000};
  nanosleep(&delay, NULL);
  if (cras_play_time) {
    unsigned long playback_latency_us;

    playback_latency_us =
        (playback_latency.tv_sec * 1000000) + (playback_latency.tv_nsec / 1000);

    fprintf(stdout, "Reported Latency: %lu uS.\n", playback_latency_us);
    fflush(stdout);
  } else {
    fprintf(stdout, "Audio not detected.\n");
  }

  /* Destruct things. */
  cras_client_stop(client);
  cras_client_stream_params_destroy(playback_params);
  if (cras_play_time) {
    free(cras_play_time);
  }
}

#endif  // TEENSY_LATENCY_TEST_INCLUDE_CRAS_HELPER_H_
