// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Client used to record or play audio by running a binary program.

#ifndef INCLUDE_BINARY_CLIENT_H_
#define INCLUDE_BINARY_CLIENT_H_

#include <set>
#include <string>
#include <vector>

#include "include/common.h"

// Player client that uses a binary program to play audio.
class PlayClient {
 public:
  explicit PlayClient(const AudioFunTestConfig &config)
      : command_(config.player_command),
        fifo_name_(config.player_fifo),
        max_num_frames_(config.fft_size),
        format_(config.sample_format),
        num_channels_(config.num_speaker_channels),
        active_channels_(config.active_speaker_channels) {}

  // Starts the client process.
  void Start();

  // Terminates the client process.
  void Terminate();

  // Plays the given block of samples.
  int Play(const std::vector<std::vector<double> > &block,
           size_t num_frames);

 private:
  int child_pid_;
  const std::string command_;
  const std::string fifo_name_;
  int play_fd_;
  size_t max_num_frames_;
  SampleFormat format_;
  size_t num_channels_;
  std::set<int> active_channels_;
};

// Recorder client that uses a binary program to record audio.
class RecordClient {
 public:
  explicit RecordClient(const AudioFunTestConfig &config)
      : command_(config.recorder_command),
        fifo_name_(config.recorder_fifo),
        format_(config.sample_format),
        num_channels_(config.num_mic_channels) {}

  // Starts the client process.
  void Start();

  // Terminates the client process.
  void Terminate();

  // Puts the recorded data into block.
  int Record(
      std::vector<std::vector<double> > *sample_ptr, size_t num_frames);

 private:
  std::string command_;
  int child_pid_;
  int record_fd_;
  const std::string fifo_name_;
  SampleFormat format_;
  size_t num_channels_;
};

#endif  // INCLUDE_BINARY_CLIENT_H_
