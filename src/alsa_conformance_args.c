/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>

#include "include/alsa_conformance_args.h"

#define MAX_DEVICE_NAME_LENGTH 20

struct alsa_conformance_args {
    char *playback_dev_name;
    unsigned int channels;
    snd_pcm_format_t format;
    unsigned int rate;
    snd_pcm_uframes_t period_size;
};

struct alsa_conformance_args *args_create()
{
    struct alsa_conformance_args *args;
    args = (struct alsa_conformance_args*)
           malloc(sizeof(struct alsa_conformance_args));
    if (!args) {
        perror("malloc (alsa_conformance_args)");
        exit(EXIT_FAILURE);
    }

    /* set default value */
    args->playback_dev_name = strdup("hw:0,0");
    args->channels = 2;
    args->format = SND_PCM_FORMAT_S16_LE;
    args->rate = 48000;
    args->period_size = 240;

    return args;
}

void args_destroy(struct alsa_conformance_args *args)
{
    free(args->playback_dev_name);
    free(args);
}

const char* args_get_playback_dev_name(const struct alsa_conformance_args *args)
{
    return args->playback_dev_name;
}

unsigned int args_get_channels(const struct alsa_conformance_args *args)
{
    return args->channels;
}

snd_pcm_format_t args_get_format(const struct alsa_conformance_args *args)
{
    return args->format;
}

unsigned int args_get_rate(const struct alsa_conformance_args *args)
{
    return args->rate;
}

snd_pcm_uframes_t args_get_period_size(const struct alsa_conformance_args *args)
{
    return args->period_size;
}

void args_set_playback_dev_name(struct alsa_conformance_args *args,
                                const char *name)
{
    free(args->playback_dev_name);
    args->playback_dev_name = strndup(name, MAX_DEVICE_NAME_LENGTH);
}

void args_set_channels(struct alsa_conformance_args *args,
                       unsigned int channels)
{
    args->channels = channels;
}

void args_set_format(struct alsa_conformance_args *args, const char *format_str)
{
    snd_pcm_format_t format;
    format = snd_pcm_format_value(format_str);
    if (format == SND_PCM_FORMAT_UNKNOWN) {
        fprintf(stderr, "unknown format: %s\n", format_str);
        exit(EXIT_FAILURE);
    }
    args->format = format;
}

void args_set_rate(struct alsa_conformance_args *args, unsigned int rate)
{
    args->rate = rate;
}

void args_set_period_size(struct alsa_conformance_args *args,
                          unsigned int period_size)
{
    args->period_size = (snd_pcm_uframes_t) period_size;
}
