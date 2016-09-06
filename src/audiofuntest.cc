// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <iostream>

#include "include/connector.h"
#include "include/evaluator.h"
#include "include/param_config.h"
#include "include/frame_generator.h"

using autotest_client::audio::Connector;
using autotest_client::audio::Evaluator;
using autotest_client::audio::FrameGenerator;
using autotest_client::audio::ParamConfig;
using autotest_client::audio::PlayClient;
using autotest_client::audio::RecordClient;

// Randomly picks an integer from the given range [min, max],
// including both end points.
inline int RandomPick(int min, int max) {
  if (min > max) {
    std::cerr << "Range error: min > max" << std::endl;
    assert(false);
  }

  static unsigned int seed = time(NULL) + getpid();
  return (rand_r(&seed) % (max - min + 1)) + min;
}

// Controls the main process of audiofuntest.
void ControlLoop(const ParamConfig &config,
                 Evaluator *evaluator,
                 RecordClient *recorder,
                 FrameGenerator *generator,
                 const int min_frequency = 4000,
                 const int max_frequency = 10000) {
  const double frequency_resolution =
      static_cast<double>(config.sample_rate) / config.fft_size;
  const int min_bin = min_frequency / frequency_resolution;
  const int max_bin = max_frequency / frequency_resolution;

  vector<int> passes(config.num_mic_channels);
  vector<bool> single_round_pass(config.num_mic_channels);

  for (int round = 1; round <= config.test_rounds; ++round) {
    std::fill(single_round_pass.begin(), single_round_pass.end(), false);
    int bin = RandomPick(min_bin, max_bin);
    double frequency = bin * frequency_resolution;

    // Sets the frequency to be generated.
    generator->SetFrequency(frequency);

    int trial = evaluator->Evaluate(recorder, bin, &single_round_pass);
    for (int chn = 0; chn < config.num_mic_channels; ++chn) {
      if (single_round_pass[chn]) {
        ++passes[chn];
      }
    }
    generator->SetStopPlayTone();

    if (config.verbose) {
      std::cout << "Frequency: " << frequency
                << " trial: " << trial << endl;
    }
    for (auto c : config.active_mic_channels) {
      const char *res = single_round_pass[c] ? "[O]" : "[X]";
      std::cout << (res)
                << " channel = " << c
                << ", success = " << passes[c]
                << ", fail = " << round - passes[c]
                << std::setprecision(4)
                << ", rate = "
                << 100.0 * passes[c] / round << "%\n";
    }
    std::cout << std::endl;
  }
}

int main(int argc, char *argv[]) {
  // Parses configuration.
  ParamConfig config;
  if (!config.ParseOptions(argc, argv)) {
    config.PrintUsage(&std::cout, argv[0]);
    return 0;
  }
  config.Print(&std::cout);

  // Main role initialization.
  FrameGenerator generator(config);

  PlayClient player(config);
  player.InitProcess(config.is_logging);

  RecordClient recorder(config);
  recorder.InitProcess(config.is_logging);

  Evaluator evaluator(config);

  generator.PlayTo(&player);

  // Starts evaluation.
  ControlLoop(config, &evaluator, &recorder, &generator);

  // Terminates and cleans up.
  recorder.Terminate();

  // Stops generator before player to
  // avoid sending tones to closed pipe.
  generator.Stop();
  player.Terminate();

  return 0;
}
