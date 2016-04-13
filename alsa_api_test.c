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
static char *play_dev = "hw:0,0";
/* Buffer size will be the maximum supported by hardware. */
static snd_pcm_uframes_t buffer_frames;
static snd_pcm_uframes_t period_size = 240;

/* Fill frames of zeros. */
static void pcm_fill_zeros(snd_pcm_t *handle, snd_pcm_uframes_t frames) {
    short *play_buf;
    int err;

    play_buf = calloc(frames * channels, sizeof(play_buf[0]));
    printf("Write %d into device\n", (int)frames);

    if ((err = snd_pcm_writei(handle, play_buf, frames))
         != frames) {
        fprintf(stderr, "write to audio interface failed (%s)\n",
                snd_strerror(err));
    }

    free(play_buf);
}

static void pcm_hw_param(snd_pcm_t *handle) {
    int err;
    snd_pcm_hw_params_t *hw_params;

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
        fprintf(stderr, "cannot set access type (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_format(handle, hw_params,
            format)) < 0) {
        fprintf(stderr, "cannot set sample format (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_rate_near(
            handle, hw_params, &rate, 0)) < 0) {
        fprintf(stderr, "cannot set sample rate (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_channels(handle, hw_params, 2)) < 0) {
        fprintf(stderr, "cannot set channel count (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    /* Makes sure buffer frames is even, or snd_pcm_hw_params will
     * return invalid argument error. */
    if ((err = snd_pcm_hw_params_get_buffer_size_max(
            hw_params, &buffer_frames)) < 0) {
        fprintf(stderr, "get buffer max (%s)\n", snd_strerror(err));
        exit(1);
    }

    buffer_frames &= ~0x01;
    if ((err = snd_pcm_hw_params_set_buffer_size_max(
            handle, hw_params, &buffer_frames)) < 0) {
        fprintf(stderr, "set_buffer_size_max (%s)\n", snd_strerror(err));
        exit(1);
    }

    printf("buffer size set to %u\n", (unsigned int)buffer_frames);

    if ((err = snd_pcm_hw_params(handle, hw_params)) < 0) {
        fprintf(stderr, "cannot set parameters (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    snd_pcm_hw_params_free(hw_params);
}

static void pcm_sw_param(snd_pcm_t *handle) {
    int err;
    snd_pcm_sw_params_t *swparams;
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

static void pcm_init(snd_pcm_t *handle)
{
    int err;
    pcm_hw_param(handle);
    pcm_sw_param(handle);

    if ((err = snd_pcm_prepare(handle)) < 0) {
        fprintf(stderr, "cannot prepare audio interface (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_start(handle)) < 0) {
        fprintf(stderr, "cannot start audio interface (%s)\n",
                snd_strerror(err));
        exit(1);
    }
}

/* Waits for target_periods periods and logs time stamp and snd_pcm_avail
 * value in each period.
 */
static void wait_for_periods(snd_pcm_t *handle, unsigned int target_periods)
{
    unsigned int num_periods = 0;
    unsigned int wake_period_us = period_size * 1E6 / rate;
    struct timespec now;
    snd_pcm_sframes_t avail_frames;
    while (num_periods < target_periods) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        printf("time: %ld.%09ld", (long)now.tv_sec, (long)now.tv_nsec);
        avail_frames = snd_pcm_avail(handle);
        printf(" state: %d, avail frames: %d, hw_level: %d\n",
               (int)snd_pcm_state(handle), (int)avail_frames,
               (int)(buffer_frames - avail_frames));
        num_periods++;
        usleep(wake_period_us);
    }
}

void check_hw_level_in_range(snd_pcm_sframes_t hw_level, int min, int max){
    printf("Expected range: %d - %d\n", min, max);
    if (hw_level <= max && hw_level >= min) {
        printf("hw_level is in the expected range\n");
    } else {
        fprintf(stderr,
                "hw_level is not in the expected range\n");
        exit(1);
    }
}


void alsa_move_test()
{
    int err;
    snd_pcm_t *handle;
    snd_pcm_sframes_t to_move, hw_level, avail_frames;
    unsigned int wait_periods = 1000;
    snd_pcm_sframes_t fuzz = 50;
    int periods_after_move = 10;

    if ((err = snd_pcm_open(&handle, play_dev,
                SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "cannot open audio device %s (%s)\n",
                play_dev, snd_strerror(err));
        exit(1);
    }

    pcm_init(handle);

    pcm_fill_zeros(handle, buffer_frames);

    wait_for_periods(handle, wait_periods);

    /* We want to move appl_ptr to hw_ptr plus fuzz such that hardware can
     * play the new samples as quick as possible.
     *    avail = buffer_frames - appl_ptr + hw_ptr
     * => hw_ptr - appl_ptr = avail - buffer_frames.
     * The difference between hw_ptr and app can be inferred from snd_pcm_avail.
     * So the amount of frames to forward appl_ptr is
     * avail - buffer_frames + fuzz.
     */
    to_move = snd_pcm_avail(handle) - buffer_frames + fuzz;
    printf("Frames to move appl_ptr forward: %d\n", (int)to_move);

    /* Move appl_ptr forward so it becomes leading hw_ptr by fuzz. */
    if ((err = snd_pcm_forward(handle, to_move)) < 0) {
        fprintf(stderr, "cannot move appl ptr forward (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    /* Checks the result after moving. The hw_level should be in the range
     * 0 - fuzz. */
    avail_frames = snd_pcm_avail(handle);
    printf("Available frames after move: %d\n", (int)avail_frames);
    hw_level = buffer_frames - avail_frames;
    printf("hw_level after moving: %d\n", (int)hw_level);

    check_hw_level_in_range(hw_level, 0, fuzz);

    /* Fills some zeros after moving to make sure PCM still plays fine. */
    pcm_fill_zeros(handle, period_size * periods_after_move);
    hw_level = buffer_frames - snd_pcm_avail(handle);
    printf("hw_level after filling %d period is %d\n",
           periods_after_move, (int)hw_level);

    wait_for_periods(handle, periods_after_move - 1);
    hw_level = buffer_frames - snd_pcm_avail(handle);
    printf("hw_level after playing %d period is %d\n",
           periods_after_move, (int)hw_level);

    /* After playing for periods_after_move - 1 periods, the hw_level
     * should be less than one period. */
    check_hw_level_in_range(hw_level, 0, period_size);
}

/* Checks if snd_pcm_drop resets the hw_ptr to 0. See bug crosbug.com/p/51882.
 */
void alsa_drop_test()
{
    int err;
    snd_pcm_t *handle;
    snd_pcm_sframes_t frames;
    snd_pcm_sframes_t fuzz = 50;
    unsigned int wait_periods = 50;

    if ((err = snd_pcm_open(
            &handle, play_dev, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "cannot open audio device %s (%s)\n",
                play_dev, snd_strerror(err));
        exit(1);
    }

    pcm_init(handle);

    pcm_fill_zeros(handle, buffer_frames);

    wait_for_periods(handle, wait_periods);

    /* Drop the samples. */
    if ((err = snd_pcm_drop(handle)) < 0) {
        fprintf(stderr, "cannot drop audio interface (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    /* Prepare and start playback again. */
    if ((err = snd_pcm_prepare(handle)) < 0) {
        fprintf(stderr, "cannot prepare audio interface (%s)\n",
                snd_strerror(err));
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

int main(int argc, char *argv[])
{
    int c, drop_test = 0, move_test = 0;
    const char *short_opt = "hd:rm";
    struct option long_opt[] =
    {
       {"help",          no_argument,       NULL, 'h'},
       {"device",        required_argument, NULL, 'd'},
       {"drop",          no_argument,       NULL, 'r'},
       {"move",          no_argument,       NULL, 'm'},
       {NULL,            0,                 NULL, 0  }
    };

    while(1) {
       c = getopt_long(argc, argv, short_opt, long_opt, NULL);
       if (c == -1)
           break;
       switch(c) {
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

       case 'h':
           printf("Usage: %s [OPTIONS]\n", argv[0]);
           printf("  --device <Device>       Device, default to hw:0,0\n");
           printf("  -h, --help              Print this help and exit\n");
           printf("  --drop                  Test snd_pcm_drop\n");
           printf("  --move                  Test snd_pcm_forward\n");
           printf("\n");
           return(0);
           break;

       case ':':
       case '?':
           fprintf(stderr, "Try `%s --help' for more information.\n",
                   argv[0]);
           exit(EXIT_FAILURE);

       default:
           fprintf(stderr, "%s: invalid option -- %c\n", argv[0], c);
           fprintf(stderr, "Try `%s --help' for more information.\n",
                   argv[0]);
           exit(EXIT_FAILURE);
       }
    }

    if (drop_test) {
        alsa_drop_test();
        exit(0);
    }

    if (move_test) {
        alsa_move_test();
        exit(0);
    }

    exit(0);
}
