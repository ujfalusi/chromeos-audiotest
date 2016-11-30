// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_COMMON_H_
#define INCLUDE_COMMON_H_

#include <assert.h>

#include <set>
#include <string>

class SampleFormat {
 public:
  enum Type {
    kPcmU8,
    kPcmS16,
    kPcmS24,
    kPcmS32,
    kPcmInvalid,
  };

  SampleFormat()
      : type_(kPcmInvalid) {}

  explicit SampleFormat(Type type)
      : type_(type) {}

  void set_type(Type type) { type_ = type; }
  Type type() const { return type_; }

  const char *to_string() const {
    switch (type_) {
      case kPcmU8:
        return "u8";
      case kPcmS16:
        return "s16";
      case kPcmS24:
        return "s24";
      case kPcmS32:
        return "s32";
      default:
        assert(false);
    }
  }

  size_t bytes() const {
    switch (type_) {
      case kPcmU8:
        return 1;
      case kPcmS16:
        return 2;
      case kPcmS24:
        return 3;
      case kPcmS32:
        return 4;
      default:
        assert(false);
    }
  }

  inline bool operator==(const SampleFormat &format) const {
    return type_ == format.type();
  }

 private:
  Type type_;
};


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
        confidence_threshold(3),
        sample_rate(64000),
        sample_format(SampleFormat::kPcmS16),
        num_mic_channels(2),
        num_speaker_channels(2),
        test_rounds(10),
        tone_length_sec(10.0f),
        verbose(false) {}

  std::set<int> active_speaker_channels;
  double allowed_delay_sec;
  int fft_size;
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
  bool verbose;
};

// Parse the active channel list. The input should be a comma separated list of
// integer. The output would be a set of integers.
void ParseActiveChannels(const char *arg, std::set<int> *output);

#endif  // INCLUDE_COMMON_H_
