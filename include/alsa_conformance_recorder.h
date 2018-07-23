/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_ALSA_CFM_RECORDER_H_
#define INCLUDE_ALSA_CFM_RECORDER_H_

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

/* Create and initialize new recorder object. */
struct alsa_conformance_recorder *recorder_create();

/* Destroy recorder object */
void recorder_destroy(struct alsa_conformance_recorder *recorder);

/* Add point (time, frames) in recorder. */
void recorder_add(struct alsa_conformance_recorder *recorder,
                  struct timespec time,
                  unsigned long frames);

/* Compute regression and print result of recorder. */
void recorder_result(struct alsa_conformance_recorder *recorder);

#endif /* INCLUDE_ALSA_CFM_RECORDER_H_ */
