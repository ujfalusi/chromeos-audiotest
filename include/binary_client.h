// Copyright 2016 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Client used to record or play audio by running a binary program.

#ifndef INCLUDE_BINARY_CLIENT_H_
#define INCLUDE_BINARY_CLIENT_H_

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "include/common.h"

// Player client that uses a binary program to play audio.
class PlayClient {
 public:
  explicit PlayClient(const AudioFunTestConfig& config);

  // Starts the client process.
  void Start();

  // Terminates the client process.
  void Terminate();

  // Plays the given block of samples.
  void Play(const void* buffer, size_t size, bool* is_stopped);

 private:
  int child_pid_;
  const std::string command_;
  const std::string fifo_name_;
  int play_fd_;
  FILE* played_file_fp_;
};

// Recorder client that uses a binary program to record audio.
class RecordClient {
 public:
  explicit RecordClient(const AudioFunTestConfig& config);

  // Starts the client process.
  void Start();

  // Terminates the client process.
  void Terminate();

  // Puts the recorded data into block.
  void Record(void* buffer, size_t size);

 private:
  std::string command_;
  int child_pid_;
  int record_fd_;
  const std::string fifo_name_;
  FILE* recorded_file_fp_;
};

#endif  // INCLUDE_BINARY_CLIENT_H_
