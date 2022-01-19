// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_COMMON_H_
#define INCLUDE_COMMON_H_

#include <pthread.h>
#include <set>
#include <string>

#include "include/sample_format.h"

struct TestConfig {
  enum TestType {
    kInvalid,
    kASharpMinorScale,
    kSingleTone,
  };

  TestConfig()
      : type(kInvalid),
        alsa_device("default"),
        format(SampleFormat::kPcmS16),
        tone_length_sec(0.3f),
        frequency(440.0f),  // Middle-A
        sample_rate(44100),
        start_volume(1.0f),
        end_volume(1.0f),
        channels(2) {
  }

  TestType type;
  std::string alsa_device;
  SampleFormat format;
  double tone_length_sec;
  double frequency;
  int sample_rate;
  double start_volume;  // TODO(ajwong): Figure out units, and use this value.
  double end_volume;
  int channels;
  std::set<int> active_channels;
};

struct AudioFunTestConfig {
  AudioFunTestConfig()
      : allowed_delay_sec(1.5),
        fft_size(2048u),
        match_window_size(7),
        power_threshold(0.01),
        confidence_threshold(3),
        sample_rate(64000),
        sample_format(SampleFormat::kPcmS16),
        num_mic_channels(2),
        num_speaker_channels(2),
        test_rounds(10),
        tone_length_sec(10.0f),
        volume_gain(50),
        min_frequency(4000),
        max_frequency(10000),
        played_file_path(),
        recorded_file_path(),
        verbose(false) {}

  std::set<int> active_speaker_channels;
  std::set<int> active_mic_channels;
  double allowed_delay_sec;
  int fft_size;
  int match_window_size;
  double power_threshold;
  double confidence_threshold;
  std::string player_command;
  std::string player_fifo;
  std::string recorder_command;
  std::string recorder_fifo;
  int sample_rate;
  SampleFormat sample_format;
  int num_mic_channels;
  int num_speaker_channels;
  int test_rounds;
  double tone_length_sec;
  int volume_gain;
  int min_frequency;
  int max_frequency;
  std::string played_file_path;
  std::string recorded_file_path;
  bool verbose;
};

// Parse the active channel list. The input should be a comma separated list of
// integer. The output would be a set of integers.
void ParseActiveChannels(const char *arg, std::set<int> *output);

#endif  // INCLUDE_COMMON_H_
