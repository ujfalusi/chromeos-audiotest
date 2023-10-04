// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LATENCY_LIBRARIES_DOLPHIN_H_
#define LATENCY_LIBRARIES_DOLPHIN_H_

#include <stdint.h>

const char* serial_path;

struct dolphin {
  int serial_fd;
};

struct dolphin* dolphin_create(const char* serial_path);
void dolphin_set_level(struct dolphin* d, int8_t level);
int dolphin_toggle_audio(struct dolphin* d);
void dolphin_destroy(struct dolphin* d);

int send_teensy_capture_start(struct dolphin* teensy_dev);
int get_teensy_capture_result(struct dolphin* teensy_dev);
int dolphin_measure_serial_latency(struct dolphin* d);

#endif  // LATENCY_LIBRARIES_DOLPHIN_H_
