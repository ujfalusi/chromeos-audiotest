/*
 * Copyright 2018 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_ALSA_CFM_RECORDER_H_
#define INCLUDE_ALSA_CFM_RECORDER_H_

#include <alsa/asoundlib.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

/* Creates and initialize new recorder object. */
struct alsa_conformance_recorder *
recorder_create(double merge_threshold_t, snd_pcm_sframes_t merge_threshold_sz);

/* Compute the median of steps. */
void recorder_compute_step_median(struct alsa_conformance_recorder *recorder);

/* Query step median */
snd_pcm_sframes_t get_step_median(struct alsa_conformance_recorder *recorder);

/* Destroys recorder object. */
void recorder_destroy(struct alsa_conformance_recorder *recorder);

/* Adds new point (time, frames) into the recorder. The return value
 * indicates whether it's merged with the previous point. */
int recorder_add(struct alsa_conformance_recorder *recorder,
		 struct timespec time, unsigned long frames);

/* Creates and initializes new recorder list. */
struct alsa_conformance_recorder_list *recorder_list_create();

/* Destroys recorder list. */
void recorder_list_destroy(struct alsa_conformance_recorder_list *list);

/* Adds new recorder into the recorder list. */
void recorder_list_add_recorder(struct alsa_conformance_recorder_list *list,
				struct alsa_conformance_recorder *recorder);

/* Prints results of recorders. */
void recorder_list_print_result(struct alsa_conformance_recorder_list *list);

#endif /* INCLUDE_ALSA_CFM_RECORDER_H_ */
