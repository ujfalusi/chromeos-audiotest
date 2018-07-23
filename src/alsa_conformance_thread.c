/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <math.h>
#include <stdbool.h>

#include "include/alsa_conformance_debug.h"
#include "include/alsa_conformance_helper.h"
#include "include/alsa_conformance_recorder.h"
#include "include/alsa_conformance_thread.h"
#include "include/alsa_conformance_timer.h"

extern int DEBUG_MODE;

struct dev_thread {
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;

    char *dev_name;
    snd_pcm_stream_t stream;
    unsigned int channels;
    snd_pcm_format_t format;
    unsigned int rate;
    snd_pcm_uframes_t period_size;
    unsigned int block_size;
    double duration;

    unsigned underrun_count; /* Record number of underruns during playback. */

    struct alsa_conformance_timer *timer;
    struct alsa_conformance_recorder *recorder;
};

struct dev_thread *dev_thread_create()
{
    struct dev_thread *thread;

    thread = (struct dev_thread*) malloc(sizeof(struct dev_thread));
    if (!thread) {
        perror("malloc (dev_thread)");
        exit(EXIT_FAILURE);
    }

    thread->handle = NULL;
    thread->params = NULL;
    thread->dev_name = NULL;
    thread->underrun_count = 0;
    thread->timer = conformance_timer_create();
    thread->recorder = recorder_create();
    return thread;
}

void dev_thread_destroy(struct dev_thread *thread)
{
    assert(thread->handle);
    free(thread->dev_name);
    snd_pcm_hw_params_free(thread->params);
    alsa_helper_close(thread->handle);
    conformance_timer_destroy(thread->timer);
    recorder_destroy(thread->recorder);
    free(thread);
}

void dev_thread_set_stream(struct dev_thread *thread, snd_pcm_stream_t stream)
{
    thread->stream = stream;
}

void dev_thread_set_dev_name(struct dev_thread *thread, const char *name)
{
    free(thread->dev_name);
    thread->dev_name = strdup(name);
}

void dev_thread_set_channels(struct dev_thread *thread, unsigned int channels)
{
    thread->channels = channels;
}

void dev_thread_set_format(struct dev_thread *thread, snd_pcm_format_t format)
{
    thread->format = format;
}

void dev_thread_set_rate(struct dev_thread *thread, unsigned int rate)
{
    thread->rate = rate;
}

void dev_thread_set_period_size(struct dev_thread *thread,
                                snd_pcm_uframes_t period_size)
{
    thread->period_size = period_size;
}

void dev_thread_set_block_size(struct dev_thread *thread, unsigned int size)
{
    thread->block_size = size;
}

void dev_thread_set_duration(struct dev_thread *thread, double duration)
{
    thread->duration = duration;
}

void dev_thread_device_open(struct dev_thread *thread)
{
    int rc;
    assert(thread->dev_name);
    rc = alsa_helper_open(thread->timer,
                          &thread->handle,
                          &thread->params,
                          thread->dev_name,
                          thread->stream);
    if (rc < 0)
        exit(EXIT_FAILURE);
}

void dev_thread_set_params(struct dev_thread *thread)
{
    int rc;
    assert(thread->handle);
    rc = alsa_helper_set_hw_params(thread->timer,
                                   thread->handle,
                                   thread->params,
                                   thread->format,
                                   thread->channels,
                                   &thread->rate,
                                   &thread->period_size);
    if (rc < 0)
        exit(EXIT_FAILURE);

    rc = alsa_helper_set_sw_param(thread->timer,
                                  thread->handle);
    if (rc < 0)
        exit(EXIT_FAILURE);
}

void dev_thread_run(struct dev_thread *thread)
{
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t period_size;
    snd_pcm_uframes_t block_size;
    snd_pcm_uframes_t frames_to_write;
    snd_pcm_sframes_t frames_avail;
    snd_pcm_sframes_t frames_written;
    snd_pcm_sframes_t frames_left;
    snd_pcm_sframes_t frames_played;
    snd_pcm_sframes_t frames_diff;
    snd_pcm_t *handle;
    struct timespec now;
    struct timespec ori;
    struct timespec relative_ts;
    struct alsa_conformance_timer *timer;
    uint8_t *buf;

    /* These variables are for debug usage. */
    char *time_str;
    struct timespec prev;
    struct timespec time_diff;
    double rate;


    handle = thread->handle;
    timer = thread->timer;
    block_size = (snd_pcm_uframes_t) thread->block_size;
    if (alsa_helper_prepare(handle) < 0)
        exit(EXIT_FAILURE);

    /* Get device buffer size. */
    snd_pcm_get_params(handle, &buffer_size, &period_size);

    /* Check block_size is available. */
    if (block_size == 0 || block_size > buffer_size / 2) {
        fprintf(stderr,
                "Block size %lu and buffer size %lu is not supported\n",
                block_size, buffer_size);
        exit(EXIT_FAILURE);
    }

    /* We need to allocate a zero buffer which will be written into device. */
    buf = (uint8_t*) calloc(block_size * 2,
                            snd_pcm_format_physical_width(thread->format) / 8
                            * thread->channels);
    if (!buf) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    /* Calculate how many frames we need to write by duration * rate. */
    frames_to_write = (snd_pcm_uframes_t)
                      round(thread->duration * (double) thread->rate);

    /* First, we write 2 blocks into buffer. */
    alsa_helper_write(handle, buf, 2 * block_size);
    frames_written = 2 * block_size;
    frames_played = 0;

    alsa_helper_start(timer, handle);

    /* Get the timestamp of beginning. */
    clock_gettime(CLOCK_MONOTONIC_RAW, &ori);

    if (DEBUG_MODE) {
        prev = ori;
        logger("%-13s %10s %10s %10s %18s\n", "TIME_DIFF(s)"
                                            , "HW_LEVEL"
                                            , "PLAYED"
                                            , "DIFF"
                                            , "RATE");
    }

    /*
     * Get available frames without sleep. It's more accurate but consume
     * more cpu time. Maybe we can choose other method in order to handle
     * multithread test in the future.
     */
    while (1) {
        frames_avail = alsa_helper_avail(timer, handle);

        frames_left = buffer_size - frames_avail;

        /*
         * Add a point into recorder when number of frames been played changes.
         */
        if (frames_played != frames_written - frames_left) {
            frames_diff = frames_written - frames_left - frames_played;
            frames_played = frames_written - frames_left;
            clock_gettime(CLOCK_MONOTONIC_RAW, &now);
            relative_ts = now;
            subtract_timespec(&relative_ts, &ori);
            recorder_add(thread->recorder, relative_ts, frames_played);

            /* In debug mode, print each point in details. */
            if (DEBUG_MODE) {
                time_diff = now;
                subtract_timespec(&time_diff, &prev);
                time_str = timespec_to_str(&time_diff);
                rate = (double) frames_diff / timespec_to_s(&time_diff);
                logger("%-13s %10ld %10ld %10ld %18lf\n", time_str
                                                        , frames_left
                                                        , frames_played
                                                        , frames_diff
                                                        , rate);
                free(time_str);
                prev = now;
            }
        }

        /*
         * Because time of writing frames into buffer is much smaller than
         * time interval of device consuming frames, we can write frames here
         * without affecting result.
         */
        if (frames_left <= (long) block_size) {
            if (frames_written >= frames_to_write)
                break;
            if (frames_left < 0)
                thread->underrun_count++;
            if (alsa_helper_write(handle, buf, block_size) < 0)
                exit(EXIT_FAILURE);
            frames_written += block_size;
        }
    }
    alsa_helper_drop(handle);
    free(buf);
}

void dev_thread_print_device_information(struct dev_thread *thread)
{
    int rc;
    assert(thread->handle);
    rc = print_device_information(thread->handle, thread->params);
    if (rc < 0)
        exit(EXIT_FAILURE);
}

void dev_thread_print_params(struct dev_thread *thread)
{
    int rc;
    assert(thread->handle);
    rc = print_params(thread->handle, thread->params);
    if (rc < 0)
        exit(EXIT_FAILURE);
}

void dev_thread_print_result(struct dev_thread* thread)
{
    conformance_timer_print_result(thread->timer);
    recorder_result(thread->recorder);
    printf("number of underrun: %u\n", thread->underrun_count);
}
