// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>

#include "include/frequency_sample_strategy.h"

namespace {
constexpr std::array<std::string_view,
                     static_cast<std::size_t>(
                         FrequencySampleStrategy::kUnknown)>
    kFrequencySampleStrategyStrings{
        "serial",
        "random",
        "step",
    };
void SeedMt19937(std::mt19937& rng) {
  /* TODO(cyueh): Log seed for reproducing errors. */
  std::array<std::mt19937::result_type, std::mt19937::state_size> state{};
  std::generate(state.begin(), state.end(), std::random_device{});
  std::seed_seq s(state.begin(), state.end());
  rng.seed(s);
}
}  // namespace

std::string_view to_string_view(FrequencySampleStrategy s) {
  auto index = static_cast<std::size_t>(s);
  if (index < kFrequencySampleStrategyStrings.size()) {
    return kFrequencySampleStrategyStrings.at(index);
  }
  return "unknown";
}

FrequencySampleStrategy from_string_view(std::string_view sv) {
  for (std::size_t index = 0; index < kFrequencySampleStrategyStrings.size();
       ++index) {
    auto& s = kFrequencySampleStrategyStrings.at(index);
    if (s == sv) {
      return static_cast<FrequencySampleStrategy>(index);
    }
  }
  return FrequencySampleStrategy::kUnknown;
}

RandomFrequencyGenerator::RandomFrequencyGenerator(int min_frequency,
                                                   int max_frequency,
                                                   int test_rounds,
                                                   double frequency_resolution)
    : rng{},
      bin_distribution{static_cast<int>(min_frequency / frequency_resolution),
                       static_cast<int>(max_frequency / frequency_resolution)} {
  SeedMt19937(rng);
}

int RandomFrequencyGenerator::GetBin(int round) {
  return bin_distribution(rng);
}

StepRandomFrequencyGenerator::StepRandomFrequencyGenerator(
    int min_frequency,
    int max_frequency,
    int test_rounds,
    double frequency_resolution)
    : min_bin(min_frequency / frequency_resolution),
      max_bin(max_frequency / frequency_resolution),
      bin_interval((max_bin + 1 - min_bin) / test_rounds),
      rng{},
      step_distribution{0, bin_interval - 1} {
  SeedMt19937(rng);
}

int StepRandomFrequencyGenerator::GetBin(int round) {
  return min_bin + (round - 1) * bin_interval + step_distribution(rng);
}

std::unique_ptr<FrequencyGenerator> make_frequency_generator(
    FrequencySampleStrategy s,
    int min_frequency,
    int max_frequency,
    int test_rounds,
    double frequency_resolution) {
  switch (s) {
    case FrequencySampleStrategy::kRandom:
      return std::make_unique<RandomFrequencyGenerator>(
          min_frequency, max_frequency, test_rounds, frequency_resolution);
    default:
    case FrequencySampleStrategy::kSerial:
      return std::make_unique<SerialFrequencyGenerator>(
          min_frequency, max_frequency, test_rounds, frequency_resolution);
    case FrequencySampleStrategy::kStep:
      return std::make_unique<StepRandomFrequencyGenerator>(
          min_frequency, max_frequency, test_rounds, frequency_resolution);
  }
}
