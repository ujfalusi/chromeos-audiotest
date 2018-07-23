/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_ALSA_CONFORMANCE_ARGS_H_
#define INCLUDE_ALSA_CONFORMANCE_ARGS_H_

#include <alsa/asoundlib.h>

/* Initialize new alsa_conformance_args object and set default value. */
struct alsa_conformance_args *args_create();

/* Destroy alsa_conformance_args object. */
void args_destroy(struct alsa_conformance_args *args);

/* Return playback device name. */
const char* args_get_playback_dev_name(
        const struct alsa_conformance_args *args);

/* Return number of channels of argument. */
unsigned int args_get_channels(const struct alsa_conformance_args *args);

/* Return format of argument. */
snd_pcm_format_t args_get_format(const struct alsa_conformance_args *args);

/* Return rate of argument. */
unsigned int args_get_rate(const struct alsa_conformance_args *args);

/* Return period size of argument. */
snd_pcm_uframes_t args_get_period_size(
        const struct alsa_conformance_args *args);

/* Set playback device name. */
void args_set_playback_dev_name(struct alsa_conformance_args *args,
                                const char *name);

/* Set channels of argument. */
void args_set_channels(struct alsa_conformance_args *args,
                       unsigned int channels);

/* Set format of argument. */
void args_set_format(struct alsa_conformance_args *args,
                     const char *format_str);

/* Set rate of argument. */
void args_set_rate(struct alsa_conformance_args *args, unsigned int rate);

/* Set period size of argument. */
void args_set_period_size(struct alsa_conformance_args *args,
                          unsigned int period_size);

#endif /* INCLUDE_ALSA_CONFORMANCE_ARGS_H_ */
