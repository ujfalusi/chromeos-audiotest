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

void args_set_playback_dev_name(struct alsa_conformance_args *args,
                                const char *name)
{
    free(args->playback_dev_name);
    args->playback_dev_name = strndup(name, MAX_DEVICE_NAME_LENGTH);
}
