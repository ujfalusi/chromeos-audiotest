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
const char *
args_get_playback_dev_name(const struct alsa_conformance_args *args);

/* Return capture device name. */
const char *args_get_capture_dev_name(const struct alsa_conformance_args *args);

/* Return number of channels of argument. */
unsigned int args_get_channels(const struct alsa_conformance_args *args);

/* Return format of argument. */
snd_pcm_format_t args_get_format(const struct alsa_conformance_args *args);

/* Return rate of argument. */
unsigned int args_get_rate(const struct alsa_conformance_args *args);

/* Return period size of argument. */
snd_pcm_uframes_t
args_get_period_size(const struct alsa_conformance_args *args);

/* Return block size of each write. */
unsigned int args_get_block_size(const struct alsa_conformance_args *args);

/* Return duration of argument. */
double args_get_duration(const struct alsa_conformance_args *args);

/* Return device file of argument. */
const char *args_get_device_file(const struct alsa_conformance_args *args);

/* Return whether it is in device info only mode. */
int args_get_dev_info_only(const struct alsa_conformance_args *args);

/* Return iterations of argument. */
int args_get_iterations(const struct alsa_conformance_args *args);

/* Set playback device name. */
void args_set_playback_dev_name(struct alsa_conformance_args *args,
				const char *name);

/* Set capture device name. */
void args_set_capture_dev_name(struct alsa_conformance_args *args,
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

/* Set block size for each write. */
void args_set_block_size(struct alsa_conformance_args *args, unsigned int size);

/* Set duration of argument. */
void args_set_duration(struct alsa_conformance_args *args, double duration);

/* Set device file of argument. */
void args_set_device_file(struct alsa_conformance_args *args, const char *name);

/* Set info only flag of argument. */
void args_set_dev_info_only(struct alsa_conformance_args *args, int flag);

/* Set iterations of argument. */
void args_set_iterations(struct alsa_conformance_args *args, int iterations);

#endif /* INCLUDE_ALSA_CONFORMANCE_ARGS_H_ */
