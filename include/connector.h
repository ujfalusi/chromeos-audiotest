// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUDIOTEST_CONNECTOR_H_
#define AUDIOTEST_CONNECTOR_H_

#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "common.h"
#include "include/param_config.h"
#include "include/util.h"

using std::cerr;

namespace autotest_client {
namespace audio {

// Interface for communicating with subprocess.
class Connector {
 public:
  // Initializes the process.
  virtual void InitProcess(bool log_to_file) = 0;

  // Terminates the process.
  virtual void Terminate() = 0;

  virtual ~Connector() {}
};

// Deals with execution and communication with player process.
class PlayClient : public Connector {
 public:
  explicit PlayClient(const ParamConfig& config)
      : player_(config.player_command),
        fifo_name_(config.player_fifo),
        max_num_frames_(config.fft_size),
        format_(config.format),
        num_channels_(config.num_speaker_channels),
        active_channels_(config.active_speaker_channels) {}

  virtual void InitProcess(bool log_to_file = false);

  virtual void Terminate();

  // Plays the given block of samples.
  int Play(const vector< vector<double> > &block, unsigned num_frames);

 private:
  Command player_;
  int child_pid_;
  int play_fd_;
  const string fifo_name_;

  unsigned max_num_frames_;
  SampleFormat format_;
  int num_channels_;
  std::set<int> active_channels_;
};

// Deals with execution and communication with recorder process.
class RecordClient : public Connector {
 public:
  explicit RecordClient(const ParamConfig& config)
      : recorder_(config.recorder_command),
        fifo_name_(config.recorder_fifo),
        format_(config.format),
        num_channels_(config.num_mic_channels) {}

  virtual void InitProcess(bool log_to_file = false);

  virtual void Terminate();

  // Puts the recorded data into block.
  int Record(vector< vector<double> > *sample_ptr, unsigned num_frames);

 private:
  Command recorder_;
  int child_pid_;
  int record_fd_;
  const string fifo_name_;

  SampleFormat format_;
  int num_channels_;
};

}  // namespace audio
}  // namespace autotest_client

#endif  // AUDIOTEST_CONNECTOR_H_
