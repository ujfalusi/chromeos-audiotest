// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LATENCY_LIBRARIES_CRAS_HELPER_H_
#define LATENCY_LIBRARIES_CRAS_HELPER_H_
#include "common.h"
#include "dolphin.h"

extern char* tty_output_dev;

void cras_test_latency();

void teensy_cras_test_latency(struct dolphin* teensy_dev);

#endif  // LATENCY_LIBRARIES_CRAS_HELPER_H_
