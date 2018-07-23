/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <alsa/asoundlib.h>

#include "include/alsa_conformance_helper.h"
#include "include/alsa_conformance_timer.h"

/* Print device information before setting params */
int print_device_information(snd_pcm_t *handle, snd_pcm_hw_params_t *params)
{
    unsigned int min;
    unsigned int max;
    unsigned int i;
    /* The min2 and max2 are for period size and buffer size. The type of
     * snd_pcm_uframes_t is unsigned long.*/
    snd_pcm_uframes_t min2;
    snd_pcm_uframes_t max2;
    int dir;
    int rc;

    puts("------DEVICE INFORMATION------");
    printf("PCM handle name: %s\n", snd_pcm_name(handle));

    printf("PCM type: %s\n", snd_pcm_type_name(snd_pcm_type(handle)));

    printf("stream: %s\n", snd_pcm_stream_name(snd_pcm_stream(handle)));

    rc = snd_pcm_hw_params_get_channels_min(params, &min);
    if (rc < 0) {
        fprintf(stderr, "hw_params_get_channels_min: %s\n", snd_strerror(rc));
        return rc;
    }

    rc = snd_pcm_hw_params_get_channels_max(params, &max);
    if (rc < 0) {
        fprintf(stderr, "hw_params_get_channels_max: %s\n", snd_strerror(rc));
        return rc;
    }

    printf("channels range: [%u, %u]\n", min, max);

    printf("available formats:");
    for (i = 0; i < SND_PCM_FORMAT_LAST; i++) {
        rc = snd_pcm_hw_params_test_format(
                handle,
                params,
                (snd_pcm_format_t) i);
        if (rc == 0)
            printf(" %s", snd_pcm_format_name((snd_pcm_format_t) i));
    }
    puts("");

    rc = snd_pcm_hw_params_get_rate_min(params, &min, &dir);
    if (rc < 0) {
        fprintf(stderr, "hw_params_get_rate_min: %s\n", snd_strerror(rc));
        return rc;
    }

    rc = snd_pcm_hw_params_get_rate_max(params, &max, &dir);
    if (rc < 0) {
        fprintf(stderr, "hw_params_get_rate_max: %s\n", snd_strerror(rc));
        return rc;
    }

    printf("rate range: [%u, %u]\n", min, max);

    printf("available rates:");
    for (i = min; i <= max; i++) {
        rc = snd_pcm_hw_params_test_rate(handle, params, i, 0);
        if (rc == 0)
            printf(" %d", i);
    }
    puts("");

    rc = snd_pcm_hw_params_get_period_size_min(params, &min2, &dir);
    if (rc < 0) {
        fprintf(stderr, "hw_params_get_period_size_min: %s\n",
                snd_strerror(rc));
        return rc;
    }

    rc = snd_pcm_hw_params_get_period_size_max(params, &max2, &dir);
    if (rc < 0) {
        fprintf(stderr, "hw_params_get_period_size_max: %s\n",
                snd_strerror(rc));
        return rc;
    }

    printf("period size range: [%lu, %lu]\n", min2, max2);

    rc = snd_pcm_hw_params_get_buffer_size_min(params, &min2);
    if (rc < 0) {
        fprintf(stderr, "hw_params_get_buffer_size_min: %s\n",
                snd_strerror(rc));
        return rc;
    }

    rc = snd_pcm_hw_params_get_buffer_size_max(params, &max2);
    if (rc < 0) {
        fprintf(stderr, "hw_params_get_buffer_size_max: %s\n",
                snd_strerror(rc));
        return rc;
    }

    printf("buffer size range: [%lu, %lu]\n", min2, max2);
    puts("------------------------------");
    return 0;
}

int print_params(snd_pcm_t *handle, snd_pcm_hw_params_t *params)
{
    unsigned int val;
    snd_pcm_uframes_t frames;
    int rc;
    int dir;

    puts("---------PRINT PARAMS---------");

    printf("PCM name: %s\n", snd_pcm_name(handle));

    printf("PCM type: %s\n", snd_pcm_type_name(snd_pcm_type(handle)));

    printf("stream: %s\n", snd_pcm_stream_name(snd_pcm_stream(handle)));

    rc = snd_pcm_hw_params_get_access(params, (snd_pcm_access_t *) &val);
    if (rc < 0) {
        fprintf(stderr, "hw_params_get_access: %s\n", snd_strerror(rc));
        return rc;
    }
    printf("access type: %s\n", snd_pcm_access_name((snd_pcm_access_t) val));

    rc = snd_pcm_hw_params_get_format(params, (snd_pcm_format_t*) &val);
    if (rc < 0) {
        fprintf(stderr, "hw_params_get_format: %s\n", snd_strerror(rc));
        return rc;
    }
    printf("format: %s\n", snd_pcm_format_name((snd_pcm_format_t) val));

    rc = snd_pcm_hw_params_get_channels(params, &val);
    if (rc < 0) {
        fprintf(stderr, "hw_params_get_channels: %s\n", snd_strerror(rc));
        return rc;
    }
    printf("channels: %d\n", val);

    rc = snd_pcm_hw_params_get_rate(params, &val, &dir);
    if (rc < 0) {
        fprintf(stderr, "hw_params_get_rate: %s\n", snd_strerror(rc));
        return rc;
    }
    printf("rate: %d bps\n", val);

    rc = snd_pcm_hw_params_get_period_time(params, &val, &dir);
    if (rc < 0) {
        fprintf(stderr, "hw_params_get_period_time: %s\n", snd_strerror(rc));
        return rc;
    }
    printf("period time: %d us\n", val);

    rc = snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    if (rc < 0) {
        fprintf(stderr, "hw_params_get_period_size: %s\n", snd_strerror(rc));
        return rc;
    }
    printf("period size: %d frames\n", frames);

    rc = snd_pcm_hw_params_get_buffer_time(params, &val, &dir);
    if (rc < 0) {
        fprintf(stderr, "hw_params_get_buffer_time: %s\n", snd_strerror(rc));
        return rc;
    }
    printf("buffer time: %d us\n", val);

    rc = snd_pcm_hw_params_get_buffer_size(params, &frames);
    if (rc < 0) {
        fprintf(stderr, "hw_params_get_buffer_time: %s\n", snd_strerror(rc));
        return rc;
    }
    printf("buffer size: %u frames\n", frames);
    puts("------------------------------");
    return 0;
}

int alsa_helper_open(struct alsa_conformance_timer *timer,
                     snd_pcm_t **handle,
                     snd_pcm_hw_params_t **params,
                     const char *dev_name,
                     snd_pcm_stream_t stream)
{
    int rc;
    conformance_timer_start(timer, SND_PCM_OPEN);
    rc = snd_pcm_open(handle,
                      dev_name,
                      stream,
                      SND_PCM_NONBLOCK |
                      SND_PCM_NO_AUTO_RESAMPLE |
                      SND_PCM_NO_AUTO_CHANNELS |
                      SND_PCM_NO_AUTO_FORMAT);
    conformance_timer_stop(timer, SND_PCM_OPEN);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_open %s: %s\n", dev_name, snd_strerror(rc));
        return rc;
    }

    /* malloc params object*/
    rc = snd_pcm_hw_params_malloc(params);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_hw_params_malloc: %s\n", snd_strerror(rc));
        return rc;
    }

    /* set default value */
    conformance_timer_start(timer, SND_PCM_HW_PARAMS_ANY);
    rc = snd_pcm_hw_params_any(*handle, *params);
    conformance_timer_stop(timer, SND_PCM_HW_PARAMS_ANY);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_hw_params_any: %s\n", snd_strerror(rc));
        return rc;
    }

    return 0;
}

int alsa_helper_close(snd_pcm_t *handle)
{
    int rc = snd_pcm_close(handle);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_close: %s\n", snd_strerror(rc));
        return rc;
    }
    return 0;
}

int alsa_helper_set_hw_params(struct alsa_conformance_timer *timer,
                              snd_pcm_t *handle,
                              snd_pcm_hw_params_t *params,
                              snd_pcm_format_t format,
                              unsigned int channels,
                              unsigned int *rate,
                              snd_pcm_uframes_t *period_size)
{
    int dir = 0;
    int rc;

    /* Disable hardware resampling. */
    rc = snd_pcm_hw_params_set_rate_resample(handle, params, 0);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_rate_resample: %s\n",
                snd_strerror(rc));
        return rc;
    }

    /* Always interleaved. */
    rc = snd_pcm_hw_params_set_access(handle, params,
                                      SND_PCM_ACCESS_MMAP_INTERLEAVED);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_access: %s\n", snd_strerror(rc));
        return rc;
    }

    rc = snd_pcm_hw_params_set_format(handle, params, format);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_format %s: %s\n",
                snd_pcm_format_name(format), snd_strerror(rc));
        return rc;
    }

    rc = snd_pcm_hw_params_set_channels(handle, params, channels);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_channels %u: %s\n", channels,
                snd_strerror(rc));
        return rc;
    }

    /* Set rate near, the rate may be changed if it's not supported */
    rc = snd_pcm_hw_params_set_rate_near(handle, params, rate, &dir);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_rate_near %u: %s\n", *rate,
                snd_strerror(rc));
        return rc;
    }

    /* Set period size near, the period size may be changed if it's not
     * supported */
    rc = snd_pcm_hw_params_set_period_size_near(handle,
                                                params,
                                                period_size,
                                                &dir);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_period_size_near %l: %s\n",
                *period_size, snd_strerror(rc));
        return rc;
    }

    /* TODO(yuhsuan): We should support setting buffer_size in the future.
     * It's set automatically now.*/

    conformance_timer_start(timer, SND_PCM_HW_PARAMS);
    rc = snd_pcm_hw_params(handle, params);
    conformance_timer_stop(timer, SND_PCM_HW_PARAMS);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_hw_params: %s\n", snd_strerror(rc));
        return rc;
    }

    return 0;
}

int alsa_helper_set_sw_param(struct alsa_conformance_timer *timer,
                             snd_pcm_t *handle)
{
    snd_pcm_sw_params_t *swparams;
    snd_pcm_uframes_t boundary;
    int rc;

    snd_pcm_sw_params_alloca(&swparams);

    rc = snd_pcm_sw_params_current(handle, swparams);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_sw_params_current: %s\n", snd_strerror(rc));
        return rc;
    }

    rc = snd_pcm_sw_params_get_boundary(swparams, &boundary);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_sw_params_get_boundary: %s\n",
                snd_strerror(rc));
        return rc;
    }

    /* Don't stop automatically. */
    rc = snd_pcm_sw_params_set_stop_threshold(handle, swparams, boundary);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_sw_params_set_stop_threshold: %s\n",
                snd_strerror(rc));
        return rc;
    }

    /* Don't start automatically. */
    rc = snd_pcm_sw_params_set_start_threshold(handle, swparams, boundary);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_sw_params_set_start_threshold: %s\n",
                snd_strerror(rc));
        return rc;
    }

    /* Disable period events. */
    rc = snd_pcm_sw_params_set_period_event(handle, swparams, 0);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_sw_params_set_period_event: %s\n",
                snd_strerror(rc));
        return rc;
    }

    conformance_timer_start(timer, SND_PCM_SW_PARAMS);
    rc = snd_pcm_sw_params(handle, swparams);
    conformance_timer_stop(timer, SND_PCM_SW_PARAMS);
    if (rc < 0) {
        fprintf(stderr, "snd_pcm_sw_params: %s\n", snd_strerror(rc));
        return rc;
    }

    return 0;
}
