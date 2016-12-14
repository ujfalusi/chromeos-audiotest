// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_GENERATOR_PLAYER_H_
#define INCLUDE_GENERATOR_PLAYER_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <thread>

#include "include/binary_client.h"
#include "include/sample_format.h"
#include "include/tone_generators.h"

class GeneratorPlayer {
 public:
  GeneratorPlayer(size_t buf_size,
                  int num_channels,
                  const std::set<int> &active_channels,
                  SampleFormat format,
                  PlayClient *player);
  void Play(ToneGenerator *generator);
  void Stop();

 private:
  size_t buf_size_;
  int num_channels_;
  const std::set<int> active_channels_;
  SampleFormat format_;
  PlayClient *player_;
  std::thread thread_;
  bool is_stopped_;
  std::unique_ptr<uint8_t[]> buffer_;

  void Run(ToneGenerator *generator);
};

#endif  // INCLUDE_GENERATOR_PLAYER_H_
