// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_FREQUENCY_SAMPLE_STRATEGY_H_
#define INCLUDE_FREQUENCY_SAMPLE_STRATEGY_H_

#include <memory>
#include <random>
#include <string_view>

enum class FrequencySampleStrategy {
  kSerial,
  kRandom,
  kStep,
  kUnknown,
};

// Convert FrequencySampleStrategy to string_view.
std::string_view to_string_view(FrequencySampleStrategy s);

// Create FrequencySampleStrategy from string_view.
FrequencySampleStrategy from_string_view(std::string_view);

class FrequencyGenerator {
 public:
  virtual ~FrequencyGenerator() {}
  virtual int GetBin(int round) = 0;
};

class SerialFrequencyGenerator : public FrequencyGenerator {
 public:
  SerialFrequencyGenerator(int min_frequency,
                           int max_frequency,
                           int test_rounds,
                           double frequency_resolution)
      : min_bin(min_frequency / frequency_resolution),
        max_bin(max_frequency / frequency_resolution),
        bin_interval((max_bin - min_bin) / (test_rounds - 1)) {}

  int GetBin(int round) override {
    return min_bin + (round - 1) * bin_interval;
  }

 private:
  int min_bin;
  int max_bin;
  int bin_interval;
};

class RandomFrequencyGenerator : public FrequencyGenerator {
 public:
  RandomFrequencyGenerator(int min_frequency,
                           int max_frequency,
                           int test_rounds,
                           double frequency_resolution);

  int GetBin(int round) override;

 private:
  std::mt19937 rng;
  std::uniform_int_distribution<int> bin_distribution;
};

class StepRandomFrequencyGenerator : public FrequencyGenerator {
 public:
  StepRandomFrequencyGenerator(int min_frequency,
                               int max_frequency,
                               int test_rounds,
                               double frequency_resolution);

  int GetBin(int round) override;

 private:
  int min_bin;
  int max_bin;
  int bin_interval;
  std::mt19937 rng;
  std::uniform_int_distribution<int> step_distribution;
};

std::unique_ptr<FrequencyGenerator> make_frequency_generator(
    FrequencySampleStrategy s,
    int min_frequency,
    int max_frequency,
    int test_rounds,
    double frequency_resolution);

#endif  // INCLUDE_FREQUENCY_SAMPLE_STRATEGY_H_
