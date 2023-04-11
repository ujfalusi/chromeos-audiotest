// Copyright 2022 The ChromiumOS Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOOPBACK_LATENCY_ALSA_HELPER_H_
#define LOOPBACK_LATENCY_ALSA_HELPER_H_

/* Measure latency from play_dev to cap_dev */
void alsa_test_latency(char* play_dev, char* cap_dev);

#endif  // LOOPBACK_LATENCY_ALSA_HELPER_H_
