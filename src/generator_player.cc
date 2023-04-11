// Copyright 2010 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/generator_player.h"

#include <stdint.h>
#include <stdio.h>

GeneratorPlayer::GeneratorPlayer(size_t buf_size,
                                 int num_channels,
                                 const std::set<int>& active_channels,
                                 SampleFormat format,
                                 PlayClient* player)
    : buf_size_(buf_size),
      num_channels_(num_channels),
      active_channels_(active_channels),
      format_(format),
      player_(player),
      is_stopped_(true),
      buffer_(new uint8_t[buf_size_]) {}

void GeneratorPlayer::Play(ToneGenerator* generator) {
  if (!is_stopped_) {
    fprintf(stderr, "Player generator is still playing.\n");
    return;
  }
  is_stopped_ = false;
  thread_ = std::thread(&GeneratorPlayer::Run, this, generator);
}

void GeneratorPlayer::Stop() {
  if (is_stopped_)
    return;
  is_stopped_ = true;
  thread_.join();
}

void GeneratorPlayer::Run(ToneGenerator* generator) {
  while (!is_stopped_ && generator->HasMoreFrames()) {
    size_t frame_read = generator->GetFrames(
        format_, num_channels_, active_channels_, buffer_.get(), buf_size_);
    player_->Play(buffer_.get(), frame_read * num_channels_ * format_.bytes(),
                  &is_stopped_);
  }
  is_stopped_ = true;
}
