// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CONNECTOR_H_
#define INCLUDE_CONNECTOR_H_

#include <set>
#include <string>
#include <vector>

#include "include/common.h"
#include "include/util.h"

// Interface for communicating with subprocess.
class Connector {
 public:
  // Initializes the process.
  virtual void InitProcess() = 0;

  // Terminates the process.
  virtual void Terminate() = 0;

  virtual ~Connector() {}
};

// Wraps the command of player & recorder and execution.
class Command {
 public:
  explicit Command(const std::string &cmd);

  // Prints the command.
  void Print(FILE *fd);

  // Executes the command.
  int Exec();

  inline bool IsSet() {
    return !exe_.empty();
  }

 private:
  std::vector<char *> exe_;
};


// Deals with execution and communication with player process.
class PlayClient : public Connector {
 public:
  explicit PlayClient(const AudioFunTestConfig &config)
      : player_(config.player_command),
        fifo_name_(config.player_fifo),
        max_num_frames_(config.fft_size),
        format_(config.sample_format),
        num_channels_(config.num_speaker_channels),
        active_channels_(config.active_speaker_channels) {}

  virtual void InitProcess();

  virtual void Terminate();

  // Plays the given block of samples.
  int Play(const std::vector<std::vector<double> > &block,
           unsigned num_frames);

 private:
  Command player_;
  int child_pid_;
  int play_fd_;
  const std::string fifo_name_;

  unsigned max_num_frames_;
  SampleFormat format_;
  int num_channels_;
  std::set<int> active_channels_;
};

// Deals with execution and communication with recorder process.
class RecordClient : public Connector {
 public:
  explicit RecordClient(const AudioFunTestConfig &config)
      : recorder_(config.recorder_command),
        fifo_name_(config.recorder_fifo),
        format_(config.sample_format),
        num_channels_(config.num_mic_channels) {}

  virtual void InitProcess();

  virtual void Terminate();

  // Puts the recorded data into block.
  int Record(
      std::vector<std::vector<double> > *sample_ptr, unsigned num_frames);

 private:
  Command recorder_;
  int child_pid_;
  int record_fd_;
  const std::string fifo_name_;

  SampleFormat format_;
  int num_channels_;
};

#endif  // INCLUDE_CONNECTOR_H_
