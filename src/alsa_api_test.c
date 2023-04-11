/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <alsa/asoundlib.h>
#include <getopt.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

static unsigned rate = 48000;
static unsigned channels = 2;
static snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
static char* play_dev = "hw:0,0";
/* Buffer size will be the maximum supported by hardware. */
static snd_pcm_uframes_t buffer_frames;
static snd_pcm_uframes_t period_size = 240;

/* Fill frames of zeros. */
static void pcm_fill(snd_pcm_t* handle,
                     snd_pcm_uframes_t frames,
                     int16_t value) {
  int16_t* play_buf;
  int err, i;

  play_buf = calloc(frames * channels, sizeof(play_buf[0]));
  for (i = 0; i < frames * channels; i++)
    play_buf[i] = value;

  printf("Write %ld of value %d into device\n", frames, (int)value);

  if ((err = snd_pcm_mmap_writei(handle, play_buf, frames)) != frames) {
    fprintf(stderr, "write to audio interface failed (%s)\n",
            snd_strerror(err));
  }

  free(play_buf);
}

static void pcm_hw_param(snd_pcm_t* handle) {
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

  if ((err = snd_pcm_hw_params_set_access(
           handle, hw_params, SND_PCM_ACCESS_MMAP_INTERLEAVED)) < 0) {
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

  /* Makes sure buffer frames is even, or snd_pcm_hw_params will
   * return invalid argument error. */
  if ((err = snd_pcm_hw_params_get_buffer_size_max(hw_params, &buffer_frames)) <
      0) {
    fprintf(stderr, "get buffer max (%s)\n", snd_strerror(err));
    exit(1);
  }

  buffer_frames &= ~0x01;
  if ((err = snd_pcm_hw_params_set_buffer_size_max(handle, hw_params,
                                                   &buffer_frames)) < 0) {
    fprintf(stderr, "set_buffer_size_max (%s)\n", snd_strerror(err));
    exit(1);
  }

  printf("buffer size set to %u\n", (unsigned int)buffer_frames);

  if ((err = snd_pcm_hw_params(handle, hw_params)) < 0) {
    fprintf(stderr, "cannot set parameters (%s)\n", snd_strerror(err));
    exit(1);
  }

  snd_pcm_hw_params_free(hw_params);
}

static void pcm_sw_param(snd_pcm_t* handle) {
  int err;
  snd_pcm_sw_params_t* swparams;
  snd_pcm_uframes_t boundary;

  snd_pcm_sw_params_alloca(&swparams);

  err = snd_pcm_sw_params_current(handle, swparams);
  if (err < 0) {
    fprintf(stderr, "sw_params_current: %s\n", snd_strerror(err));
    exit(1);
  }

  err = snd_pcm_sw_params_get_boundary(swparams, &boundary);
  if (err < 0) {
    fprintf(stderr, "get_boundary: %s\n", snd_strerror(err));
    exit(1);
  }
  printf("boundary = %lu\n", boundary);

  err = snd_pcm_sw_params_set_stop_threshold(handle, swparams, boundary);
  if (err < 0) {
    fprintf(stderr, "set_stop_threshold: %s\n", snd_strerror(err));
    exit(1);
  }

  /* Don't auto start. */
  err = snd_pcm_sw_params_set_start_threshold(handle, swparams, boundary);
  if (err < 0) {
    fprintf(stderr, "set_stop_threshold: %s\n", snd_strerror(err));
    exit(1);
  }

  /* Disable period events. */
  err = snd_pcm_sw_params_set_period_event(handle, swparams, 0);
  if (err < 0) {
    fprintf(stderr, "set_period_event: %s\n", snd_strerror(err));
    exit(1);
  }

  err = snd_pcm_sw_params(handle, swparams);
  if (err < 0) {
    fprintf(stderr, "sw_params: %s\n", snd_strerror(err));
    exit(1);
  }
}

static void pcm_init(snd_pcm_t* handle) {
  int err;
  pcm_hw_param(handle);
  pcm_sw_param(handle);

  if ((err = snd_pcm_prepare(handle)) < 0) {
    fprintf(stderr, "cannot prepare audio interface (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_start(handle)) < 0) {
    fprintf(stderr, "cannot start audio interface (%s)\n", snd_strerror(err));
    exit(1);
  }
}

/* Waits for target_periods periods and logs time stamp and snd_pcm_avail
 * value in each period.
 */
static void wait_for_periods(snd_pcm_t* handle, unsigned int target_periods) {
  unsigned int num_periods = 0;
  unsigned int wake_period_us = period_size * 1E6 / rate;
  struct timespec now;
  snd_pcm_sframes_t avail_frames;
  while (1) {
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
    printf("time: %ld.%09ld", (long)now.tv_sec, (long)now.tv_nsec);
    avail_frames = snd_pcm_avail(handle);
    printf(" state: %d, avail frames: %ld, hw_level: %ld\n",
           (int)snd_pcm_state(handle), avail_frames,
           buffer_frames - avail_frames);
    /* Breaks here so we can print the last timestamp and frames. */
    if (num_periods == target_periods)
      break;
    num_periods++;
    usleep(wake_period_us);
  }
}

void check_hw_level_in_range(snd_pcm_sframes_t hw_level,
                             snd_pcm_sframes_t min,
                             snd_pcm_sframes_t max) {
  printf("Expected range: %ld - %ld\n", min, max);
  if (hw_level <= max && hw_level >= min) {
    printf("hw_level is in the expected range\n");
  } else {
    fprintf(stderr, "hw_level is not in the expected range\n");
    exit(1);
  }
}

void move_appl_ptr(snd_pcm_t* handle, snd_pcm_sframes_t fuzz) {
  int err = 0;
  snd_pcm_sframes_t to_move, hw_level, avail_frames;

  avail_frames = snd_pcm_avail(handle);
  printf("Available frames: %ld\n", avail_frames);
  hw_level = buffer_frames - avail_frames;
  printf("hw_level frames: %ld\n", hw_level);

  /* We want to move appl_ptr to hw_ptr plus fuzz such that hardware can
   * play the new samples as quick as possible.
   * The difference between hw_ptr and app can be inferred from snd_pcm_avail.
   *    avail = buffer_frames - appl_ptr + hw_ptr
   * => hw_ptr - appl_ptr = avail - buffer_frames.
   * The amount to forward is fuzz - hw_level = fuzz - appl_ptr - hw_ptr.
   * Depending on the sign of this value, we need to forward or rewind
   * appl_ptr. Check go/cros-low-latency for detailed explanation.
   */
  to_move = fuzz + avail_frames - buffer_frames;
  if (to_move > 0) {
    printf("forward by %ld\n", to_move);
    err = snd_pcm_forward(handle, to_move);
  } else if (to_move < 0) {
    printf("rewind by %ld\n", -to_move);
    err = snd_pcm_rewind(handle, -to_move);
  } else {
    printf("no need to move\n");
    return;
  }

  if (err < 0) {
    fprintf(stderr, "cannot move appl ptr (%s)\n", snd_strerror(err));
    exit(1);
  }
}

void check_appl_ptr(snd_pcm_t* handle, snd_pcm_sframes_t fuzz) {
  snd_pcm_sframes_t hw_level, avail_frames;
  int periods_after_move;
  struct timespec time_1, time_2;
  snd_pcm_sframes_t level_1, level_2;
  float time_diff, measure_rate;

  /* Checks the result after moving. The hw_level should be in the range
   * 0 - fuzz. */
  avail_frames = snd_pcm_avail(handle);
  printf("Available frames after move: %ld\n", avail_frames);
  hw_level = buffer_frames - avail_frames;
  printf("hw_level after moving: %ld\n", hw_level);

  check_hw_level_in_range(hw_level, 0, fuzz);

  /* Fills some zeros after moving to make sure PCM still plays fine.
   * Sets periods_after_move so device will play half of buffer size.
   * This would result in an accurate estimated sampling rate. */
  periods_after_move = (buffer_frames >> 1) / period_size;
  printf("Test playback for %d periods after move\n", periods_after_move);
  pcm_fill(handle, period_size * periods_after_move, 0);
  clock_gettime(CLOCK_MONOTONIC_RAW, &time_1);
  printf("time: %ld.%09ld", (long)time_1.tv_sec, (long)time_1.tv_nsec);
  level_1 = buffer_frames - snd_pcm_avail(handle);
  printf(" hw_level after filling %d period is %ld\n", periods_after_move,
         level_1);

  wait_for_periods(handle, periods_after_move - 1);

  clock_gettime(CLOCK_MONOTONIC_RAW, &time_2);
  printf("time: %ld.%09ld", (long)time_2.tv_sec, (long)time_2.tv_nsec);
  level_2 = buffer_frames - snd_pcm_avail(handle);
  printf(" hw_level after playing %d period is %ld\n", periods_after_move - 1,
         level_2);

  /* Checks the device consumption rate in this duration is reasonable. */
  time_diff = (time_2.tv_sec - time_1.tv_sec) +
              (float)(time_2.tv_nsec - time_1.tv_nsec) * 1E-9;
  measure_rate = (level_1 - level_2) / time_diff;

  if (fabsf(measure_rate - rate) <= 1000) {
    printf("rate %f is in the expected range near %u\n", measure_rate, rate);
  } else {
    fprintf(stderr, "rate %f is not in the expected range near %u\n",
            measure_rate, rate);
    exit(1);
  }
}

void move_and_check(snd_pcm_t* handle, snd_pcm_sframes_t fuzz) {
  move_appl_ptr(handle, fuzz);
  check_appl_ptr(handle, fuzz);
}

void alsa_move_test(unsigned int wait_periods) {
  int err;
  snd_pcm_t* handle;
  snd_pcm_sframes_t fuzz = 50;

  if ((err = snd_pcm_open(&handle, play_dev, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    fprintf(stderr, "cannot open audio device %s (%s)\n", play_dev,
            snd_strerror(err));
    exit(1);
  }

  pcm_init(handle);

  pcm_fill(handle, buffer_frames, 0);

  wait_for_periods(handle, wait_periods);

  move_and_check(handle, fuzz);

  if ((err = snd_pcm_close(handle)) < 0) {
    fprintf(stderr, "cannot close device (%s)\n", snd_strerror(err));
    exit(1);
  }
}

/* Checks if snd_pcm_drop resets the hw_ptr to 0. See bug crosbug.com/p/51882.
 */
void alsa_drop_test() {
  int err;
  snd_pcm_t* handle;
  snd_pcm_sframes_t frames;
  snd_pcm_sframes_t fuzz = 50;
  unsigned int wait_periods = 50;

  if ((err = snd_pcm_open(&handle, play_dev, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    fprintf(stderr, "cannot open audio device %s (%s)\n", play_dev,
            snd_strerror(err));
    exit(1);
  }

  pcm_init(handle);

  pcm_fill(handle, buffer_frames, 0);

  wait_for_periods(handle, wait_periods);

  /* Drop the samples. */
  if ((err = snd_pcm_drop(handle)) < 0) {
    fprintf(stderr, "cannot drop audio interface (%s)\n", snd_strerror(err));
    exit(1);
  }

  /* Prepare and start playback again. */
  if ((err = snd_pcm_prepare(handle)) < 0) {
    fprintf(stderr, "cannot prepare audio interface (%s)\n", snd_strerror(err));
    exit(1);
  }

  if ((err = snd_pcm_start(handle)) < 0) {
    fprintf(stderr, "cannot start for the second time (%s)\n",
            snd_strerror(err));
    exit(1);
  }

  /* The avail should be a reasonable value that is slightly larger than
   * buffer level. avail = buffer - (appl_ptr - hw_ptr).
   * The expected behavior:
   * snd_pcm_drop: hw_ptr should be 0.
   * snd_pcm_prepare: appl_ptr should be the same as hw_ptr, which is 0.
   * snd_pcm_start: hw_ptr gets synced to hardware, should be a small number.
   * So, the new avail should be slightly larger than buffer. */
  frames = snd_pcm_avail(handle);

  printf("Avail frames after drop, prepare, start: %d\n", (int)frames);

  if ((err = snd_pcm_close(handle)) < 0) {
    fprintf(stderr, "cannot close device (%s)\n", snd_strerror(err));
    exit(1);
  }

  printf("Expected avail frames after drop, prepare, start is 0 - %d\n",
         (int)(buffer_frames + fuzz));

  if (frames < 0 || frames > buffer_frames + fuzz) {
    fprintf(stderr, "Avail frames after drop, prepare, start is %d\n",
            (int)frames);
    exit(1);
  }
}

void alsa_fill_test() {
  int err;
  snd_pcm_t* handle;
  unsigned int wait_periods = 10;
  const snd_pcm_channel_area_t* my_areas;
  snd_pcm_uframes_t offset, frames;
  int16_t *dst, *zeros;
  int n_bytes;

  if ((err = snd_pcm_open(&handle, play_dev, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    fprintf(stderr, "cannot open audio device %s (%s)\n", play_dev,
            snd_strerror(err));
    exit(1);
  }

  pcm_init(handle);

  /* Write nonzero values into buffer. */
  pcm_fill(handle, buffer_frames, 1);

  /* Play for some periods. */
  wait_for_periods(handle, wait_periods);

  /* Get the mmap area. */
  err = snd_pcm_mmap_begin(handle, &my_areas, &offset, &frames);
  if (err < 0) {
    fprintf(stderr, "cannot mmap begin (%s)\n", snd_strerror(err));
    exit(1);
  }

  /* Fill whole buffer with zeros without committing it.
   * The number of bytes is buffer_frames * channel * 2 (16 bit sample) */
  n_bytes = buffer_frames * channels * 2;
  memset((int8_t*)my_areas[0].addr, 0, n_bytes);
  printf("Filled mmap buffer with zeros\n");

  /* Play for some periods. */
  wait_for_periods(handle, wait_periods);

  /* Check the samples in buffer are all zeros. */
  err = snd_pcm_mmap_begin(handle, &my_areas, &offset, &frames);
  if (err < 0) {
    fprintf(stderr, "cannot mmap begin the second time (%s)\n",
            snd_strerror(err));
    exit(1);
  }
  dst = (int16_t*)my_areas[0].addr;

  zeros = calloc(buffer_frames * channels, sizeof(zeros[0]));

  if (memcmp(zeros, dst, n_bytes)) {
    fprintf(stderr, "buffer is not all zeros\n");
    free(zeros);
    exit(1);
  }
  free(zeros);
  printf("Buffer is filled with zeros\n");
}

int main(int argc, char* argv[]) {
  int c, drop_test = 0, move_test = 0, fill_test = 0;
  const char* short_opt = "hd:rm";
  struct option long_opt[] = {{"help", no_argument, NULL, 'h'},
                              {"device", required_argument, NULL, 'd'},
                              {"drop", no_argument, NULL, 'r'},
                              {"move", no_argument, NULL, 'm'},
                              {"fill", no_argument, NULL, 'f'},
                              {NULL, 0, NULL, 0}};

  while (1) {
    c = getopt_long(argc, argv, short_opt, long_opt, NULL);
    if (c == -1)
      break;
    switch (c) {
      case 'd':
        play_dev = optarg;
        printf("Assign play_dev to %s\n", play_dev);
        break;

      case 'r':
        drop_test = 1;
        printf("Test snd_pcm_drop\n");
        break;

      case 'm':
        move_test = 1;
        printf("Test snd_pcm_forward\n");
        break;

      case 'f':
        fill_test = 1;
        printf("Test snd_pcm_mmap_begin and filling buffer.\n");
        break;

      case 'h':
        printf("Usage: %s [OPTIONS]\n", argv[0]);
        printf("  --device <Device>       Device, default to hw:0,0\n");
        printf("  -h, --help              Print this help and exit\n");
        printf("  --drop                  Test snd_pcm_drop\n");
        printf(
            "  --move                  Test snd_pcm_rewind and "
            "snd_pcm_forward\n");
        printf("  --fill                  Test snd_pcm_mmap_begin\n");
        printf("\n");
        return (0);
        break;

      case ':':
      case '?':
        fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
        exit(EXIT_FAILURE);

      default:
        fprintf(stderr, "%s: invalid option -- %c\n", argv[0], c);
        fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  if (drop_test) {
    alsa_drop_test();
    exit(0);
  }

  if (move_test) {
    /* Test rewind and forward.
     * - Waiting 10 periods: appl_ptr is still ahead of hw_ptr, test
     *                       snd_pcm_rewind call.
     * - Waiting 1000 periods: hw_ptr is ahead of appl_ptr, test
     *                         snd_pcm_forward call.
     */
    alsa_move_test(10);
    alsa_move_test(1000);
    exit(0);
  }

  if (fill_test) {
    alsa_fill_test();
    exit(0);
  }

  exit(0);
}
