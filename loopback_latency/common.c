// Copyright 2022 The ChromiumOS Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <alsa/asoundlib.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>

#include "args.h"

double phase = M_PI / 2;
pthread_mutex_t latency_test_mutex;
pthread_cond_t sine_start;
int terminate_playback;
int terminate_capture;
int sine_started;

int capture_count;
int playback_count;

void generate_sine(const snd_pcm_channel_area_t *areas,
                   snd_pcm_uframes_t offset, int count,
                   double *_phase)
{
    static double max_phase = 2. * M_PI;
    double phase = *_phase;
    double step = max_phase * 1000 / (double)rate;
    unsigned char *samples[channels];
    int steps[channels];
    unsigned int chn;
    int format_bits = snd_pcm_format_width(format);
    unsigned int maxval = (1 << (format_bits - 1)) - 1;
    int bps = format_bits / 8;  /* bytes per sample */
    int phys_bps = snd_pcm_format_physical_width(format) / 8;
    int big_endian = snd_pcm_format_big_endian(format) == 1;
    int to_unsigned = snd_pcm_format_unsigned(format) == 1;
    int is_float = (format == SND_PCM_FORMAT_FLOAT_LE ||
            format == SND_PCM_FORMAT_FLOAT_BE);

    /* Verify and prepare the contents of areas */
    for (chn = 0; chn < channels; chn++) {
        if ((areas[chn].first % 8) != 0) {
            fprintf(stderr, "areas[%i].first == %i, aborting...\n", chn,
                    areas[chn].first);
            exit(EXIT_FAILURE);
        }
        if ((areas[chn].step % 16) != 0) {
            fprintf(stderr, "areas[%i].step == %i, aborting...\n", chn, areas
                    [chn].step);
            exit(EXIT_FAILURE);
        }
        steps[chn] = areas[chn].step / 8;
        samples[chn] = ((unsigned char *)areas[chn].addr) +
                (areas[chn].first / 8) + offset * steps[chn];
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
        } else
            res = sin(phase) * maxval;
        if (to_unsigned)
            res ^= 1U << (format_bits - 1);
        for (chn = 0; chn < channels; chn++) {
            /* Generate data based on endian format */
            if (big_endian) {
                for (i = 0; i < bps; i++)
                    *(samples[chn] + phys_bps - 1 - i) = (res >> i * 8) & 0xff;
            } else {
                for (i = 0; i < bps; i++)
                    *(samples[chn] + i) = (res >>  i * 8) & 0xff;
            }
            samples[chn] += steps[chn];
        }
        phase += step;
        if (phase >= max_phase)
            phase -= max_phase;
    }
    *_phase = phase;
}

/* Looks for the first sample in buffer whose absolute value exceeds
 * noise_threshold. Returns the index of found sample in frames, -1
 * if not found. */
int check_for_noise(short *buf, unsigned len, unsigned channels)
{
    unsigned int i;
    for (i = 0; i < len * channels; i++)
        if (abs(buf[i]) > noise_threshold)
            return i / channels;
    return -1;
}

unsigned long subtract_timevals(const struct timeval *end,
                                const struct timeval *beg)
{
    struct timeval diff;
    /* If end is before geb, return 0. */
    if ((end->tv_sec < beg->tv_sec) ||
            ((end->tv_sec == beg->tv_sec) && (end->tv_usec <= beg->tv_usec)))
        diff.tv_sec = diff.tv_usec = 0;
    else {
        if (end->tv_usec < beg->tv_usec) {
            diff.tv_sec = end->tv_sec - beg->tv_sec - 1;
            diff.tv_usec =
                end->tv_usec + 1000000L - beg->tv_usec;
        } else {
            diff.tv_sec = end->tv_sec - beg->tv_sec;
            diff.tv_usec = end->tv_usec - beg->tv_usec;
        }
    }
    return diff.tv_sec * 1000000 + diff.tv_usec;
}
