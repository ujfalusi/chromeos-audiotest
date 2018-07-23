/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <alsa/asoundlib.h>

#include "include/alsa_conformance_helper.h"

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

int alsa_helper_open(snd_pcm_t **handle,
                     snd_pcm_hw_params_t **params,
                     const char *dev_name,
                     snd_pcm_stream_t stream)
{
    int rc;
    rc = snd_pcm_open(handle,
                      dev_name,
                      stream,
                      SND_PCM_NONBLOCK |
                      SND_PCM_NO_AUTO_RESAMPLE |
                      SND_PCM_NO_AUTO_CHANNELS |
                      SND_PCM_NO_AUTO_FORMAT);
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
    rc = snd_pcm_hw_params_any(*handle, *params);
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
