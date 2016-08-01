// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUDIOTEST_PARAM_CONFIG_H_
#define AUDIOTEST_PARAM_CONFIG_H_


// Parses config file for running player & recorder.
//
// * configuration file name default : test.conf

// * configuration file format :
//   each line contains an argument setting, with the format:
//   [argument name] = [value]
//
//   Available arguments:
//       player-proc, recorder-proc, sample-rate, fftsize,
//       mic-channels, speaker-channels,
//       active-mic-channels, active-speaker-channels,
//       tone-length, start-volume, end-volume
//       player-fifo, recorder-fifo, verbose, log
//       test-rounds, pass-threshold, sample-format

// * Example configuration file for platform with sox:
//   """
//   player-proc = play -t raw -c 2 -2 -r 48000 -s -
//   recorder-proc = rec -t raw -c 2 -2 -r 48000 -s -
//   """

// * Example configuration file for platform with tinycap/tinyplay (Android)
//   (be sure to run audiofuntest with -f play.fifo -F cap.fifo)
//   """
//   tinyplay play.fifo -raw -r 48000 -c 2 -b 16
//   tinycap cap.fifo -raw -r 48000 -c 2 -b 16
//   """

#include <assert.h>
#include <getopt.h>
#include <unistd.h>

#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <set>
#include <string>
#include <vector>

#include "common.h"

using std::string;
using std::endl;

namespace autotest_client {
namespace audio {

// Wraps the command of player & recorder and execution.
class Command {
 public:
  Command() {}
  // Parses arguments from lines in config file.
  void Parse(const char *line);

  // Prints the command.
  void Print(std::ostream *out);

  // Executes the command.
  int Exec() {
    exe_.push_back(NULL);
    return execvp(exe_[0], (char* const*)&exe_[0]);
  }

  inline bool IsSet() {
    return !exe_.empty();
  }

 private:
  std::vector<char *> exe_;
};

// All parameters-related methods here,
// 1. the parsing of arguments.
// 2. usage of parameters helps methods.
class ParamConfig {
 public:
  ParamConfig()
      : player_command(),
        recorder_command(),
        tone_length(3.0),
        sample_rate(48000),
        start_volume(1.0),
        end_volume(1.0),
        num_mic_channels(2),
        num_speaker_channels(2),
        fft_size(2048),
        format(SampleFormat::kPcmS16),
        test_rounds(20),
        pass_threshold(3.0),
        allowed_delay_millisecs(1200),
        verbose(false),
        is_logging(false) {}

  // Parses all options in argv.
  bool ParseOptions(int argc, char* const argv[]);

  // Prints all options.
  void Print(std::ostream *out);

  // Prints the usage of all possible parameters.
  static void PrintUsage(std::ostream *out, const char* name);

  Command player_command;
  Command recorder_command;

  string player_fifo;
  string recorder_fifo;

  double tone_length;
  int sample_rate;
  double start_volume, end_volume;

  int num_mic_channels;
  std::set<int> active_mic_channels;

  int num_speaker_channels;
  std::set<int> active_speaker_channels;
  int fft_size;

  SampleFormat format;

  int test_rounds;
  double pass_threshold;
  int allowed_delay_millisecs;

  bool verbose;
  bool is_logging;


 private:
  // Parses the active channels split by comma(,).
  void ParseActiveChannels(const char* arg, bool is_mic);

  // Parses configuration file and wrap it in command object.
  bool ParseConfigFile(const char *config_file);

  // Parses boolean option string to bool value.
  bool ParseBool(const string& str);

  // Parses format string to fit SampleFormat.
  void ParseSampleFormat(const char* arg);

  constexpr static const char *short_options_ =
      "z:l:r:s:e:c:C:a:A:t:n:f:F:T:p:d:vhL";

  constexpr static const struct option long_options_[] = {
    {"config-file", 1, NULL, 'z'},
    {"tone-length", 1, NULL, 'l'},
    {"sample-rate", 1, NULL, 'r'},
    {"start-volume", 1, NULL, 's'},
    {"end-volume", 1, NULL, 'e'},
    {"mic-channels", 1, NULL, 'c'},
    {"speaker-channels", 1, NULL, 'C'},
    {"active-mic-channels", 1, NULL, 'a'},
    {"active-speaker-channels", 1, NULL, 'A'},
    {"fftsize", 1, NULL, 'n'},
    {"player-fifo", 1, NULL, 'f'},
    {"recorder-fifo", 1, NULL, 'F'},
    {"test-rounds", 1, NULL, 't'},
    {"pass-threshold", 1, NULL, 'p'},
    {"allowed-delay", 1, NULL, 'd'},
    {"help", 0, NULL, 'h'},
    {"verbose", 0, NULL, 'v'},
    {"log", 0, NULL, 'L'}
  };
};

}  // namespace audio
}  // namespace autotest_client

#endif  // AUDIOTEST_PARAM_CONFIG_H_
