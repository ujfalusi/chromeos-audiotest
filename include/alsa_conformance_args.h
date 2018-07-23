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

/* Set playback device name. */
void args_set_playback_dev_name(struct alsa_conformance_args *args,
                                const char *name);

#endif /* INCLUDE_ALSA_CONFORMANCE_ARGS_H_ */
