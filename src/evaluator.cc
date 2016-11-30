// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/evaluator.h"

#include <algorithm>
#include <iomanip>

#include "include/util.h"

Evaluator::Evaluator(const AudioFunTestConfig &config, int range)
    : filter_(2 * range + 1),
      bin_range_(range),
      num_channels_(config.num_mic_channels),
      buffer_size_(config.fft_size),
      format_(config.sample_format),
      sample_rate_(config.sample_rate),
      bin_(range * 2 + 1),
      confidence_threshold_(config.confidence_threshold),
      is_debug_(config.verbose) {
  // Initializes original expected filter.
  filter_[bin_range_] = 1;  // center
  double mean = 1.0;
  double sigma = 1.0;  // standard deviation

  // Normalization.
  mean /= filter_.size();
  sigma = sqrt(sigma / filter_.size() - mean * mean);
  for (auto &x : filter_) {
    x = (x - mean) / sigma;
  }

  // Estimates max trial.
  // max_trial_ is the reverse of allowed delay plus the threshold to pass.
  // And +2 is for the acceptable variation.
  max_trial_ =
      config.allowed_delay_sec * sample_rate_ / buffer_size_
      + confidence_threshold_ + 2;
}


// If passed all channel then return true, otherwise false.
// It will automatically record the current accumulate confidence
// for the set frequency.
int Evaluator::Evaluate(RecordClient *recorder,
                        int center_bin,
                        std::vector<bool> *single_output) {
  bool freq_pass_all;
  auto &single_round = *single_output;
  std::vector< std::vector<double> > buffer(
      num_channels_, std::vector<double>(buffer_size_));

  std::vector<double> accum_confidence(num_channels_);

  int trial;
  for (trial = 1;
       trial <= max_trial_ && !freq_pass_all;
       ++trial) {
    freq_pass_all = true;
    if (recorder->Record(&buffer, buffer_size_) <= 0) {
      fprintf(stderr, "Retrieve recorded data error.\n");
      assert(false);
    }

    // Extends samples to complex samples.
    std::vector< std::vector<double> > complex_sample(
        num_channels_, std::vector<double>(buffer_size_ * 2));
    for (int ch = 0; ch < num_channels_; ++ch) {
      auto ptr = complex_sample[ch].begin();
      for (double s : buffer[ch]) {
        *(ptr++) = s;
        *(ptr++) = 0.0;
      }
    }

    // Evaluates all channels.
    for (int channel = 0; channel < num_channels_; ++channel) {
      if (accum_confidence[channel] >= confidence_threshold_) continue;

      accum_confidence[channel] += std::max(
          EstimateChannel(&(complex_sample[channel]), center_bin), 0.0);
      if (accum_confidence[channel] < confidence_threshold_) {
        freq_pass_all = false;
      } else {
        single_round[channel] = true;
      }
    }
  }

  return trial;
}

double Evaluator::EstimateChannel(
    std::vector<double> *cell_ptr, int center_bin) {
  FFT(cell_ptr);
  auto &cell = *cell_ptr;

  double confidence = 0.0, mean = 0.0, sigma = 0.0;

  for (int bin = (center_bin - bin_range_);
       bin <= (center_bin + static_cast<int>(bin_range_));
       ++bin) {
    int index = bin - (center_bin - bin_range_);
    bin_[index] = SquareAbs(cell[2 * bin], cell[2 * bin + 1]) / cell.size();
    if (is_debug_)
      printf("%e ", bin_[index]);
    confidence += bin_[index] * filter_[index];
    mean += bin_[index];
    sigma += bin_[index] * bin_[index];
  }
  if (is_debug_) printf("\n");
  // Avoids divide by zero.
  if (std::abs(sigma) < 1e-9) {
    return 0.0;
  }
  const double power_ratio = bin_[bin_range_] / mean;
  mean /= filter_.size();
  sigma = sqrt(sigma / filter_.size() - mean * mean);
  confidence /= (sigma * filter_.size());
  if (is_debug_)
    printf("power: %0.4f, conf: %0.4f\n", power_ratio, confidence);
  return power_ratio * confidence;
}

void Evaluator::FFT(std::vector<double> *data_ptr) const {
  auto &data = *data_ptr;
  unsigned order, pos = 1, size = data.size();

  // Reverses binary reindexing.
  for (unsigned i = 1; i < size; i += 2) {
    if (pos > i) {
      std::swap(data[pos - 1], data[i - 1]);
      std::swap(data[pos], data[i]);
    }
    order = size / 2;
    while (order >= 2 && pos > order) {
      pos -= order;
      order >>= 1;
    }
    pos += order;
  }

  // Danielson-Lanczos lemma.
  unsigned mmax = 2, step;
  double theta, wtemp;
  double cur[2], pre[2], temp[2];  // [0] real, [1] complex

  mmax = 2;
  while (size > mmax) {
    step = mmax << 1;
    theta = -(2 * M_PI / mmax);
    wtemp = sin(theta / 2);

    pre[0] = -2.0 * wtemp * wtemp;
    pre[1] = sin(theta);

    cur[0] = 1.0;
    cur[1] = 0.0;
    for (unsigned k = 1; k < mmax; k += 2) {
      for (unsigned i = k; i <= size; i += step) {
        pos = i + mmax;
        temp[0] = cur[0] * data[pos - 1] - cur[1] * data[pos];
        temp[1] = cur[0] * data[pos] + cur[1] * data[pos - 1];

        data[pos - 1] = data[i - 1] - temp[0];
        data[pos] = data[i] - temp[1];
        data[i - 1] += temp[0];
        data[i] += temp[1];
      }
      wtemp = cur[0];
      cur[0] += wtemp * pre[0] - cur[1] * pre[1];
      cur[1] += cur[1] * pre[0] + wtemp * pre[1];
    }
    mmax = step;
  }
}
