// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/evaluator.h"

#include <cmath>

namespace {
// Returns the square of absolute value of complex number.
inline double SquareAbs(const double real, const double imaginary) {
  return real * real + imaginary * imaginary;
}

void ToComplex(const std::vector<double> &data, std::vector<double> *output) {
  if (output->size() != data.size() * 2)
    output->resize(data.size() * 2);
  auto it = output->begin();
  for (size_t i = 0; i < data.size(); ++i) {
    *(it++) = data[i];
    *(it++) = 0.0;
  }
}

// One dimension fast fourier transform using Danielson-Lanczos lemma.
void FFT(std::vector<double> *data_ptr) {
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

}  // namespace

Evaluator::Evaluator(const AudioFunTestConfig& config)
    : filter_(config.match_window_size),
      half_window_size_(config.match_window_size / 2),
      num_channels_(config.num_mic_channels),
      active_mic_channels_(config.active_mic_channels),
      format_(config.sample_format),
      sample_rate_(config.sample_rate),
      bin_(config.match_window_size),
      power_threshold_(config.power_threshold),
      confidence_threshold_(config.confidence_threshold),
      verbose_(config.verbose) {
  // Initializes original expected filter.
  filter_[half_window_size_] = 1;  // center
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
      config.allowed_delay_sec * sample_rate_ / config.fft_size
      + confidence_threshold_ + 2;

  buf_size_ = num_channels_ * config.fft_size * format_.bytes();
  buffer_.reset(new uint8_t[buf_size_]);
}

void Evaluator::Evaluate(int center_bin,
                         RecordClient *recorder,
                         std::vector<bool> *result) {
  bool all_pass = false;
  std::vector<double> accum_confidence(num_channels_);

  for (int trial = 1;
       trial <= max_trial_ && !all_pass;
       ++trial) {
    all_pass = true;
    recorder->Record(buffer_.get(), buf_size_);

    std::vector<std::vector<double> > data;
    int num_frames = Unpack(
        buffer_.get(), buf_size_, format_, num_channels_, &data);

    std::vector<double> complex_data(num_frames * 2);

    // Evaluates all channels.
    for (int channel: active_mic_channels_) {
      if (accum_confidence[channel] >= confidence_threshold_)
        continue;
      ToComplex(data[channel], &complex_data);
      accum_confidence[channel] += std::max(
          EstimateChannel(&complex_data, center_bin), 0.0);
      if (accum_confidence[channel] < confidence_threshold_)
        all_pass = false;
      else
        (*result)[channel] = true;
    }
  }
}

double Evaluator::EstimateChannel(
    std::vector<double> *data, int center_bin) {
  // Calculate RMS before FFT
  // The |data| here is already formatted as double sized to store the result
  // (read, imaginary) from FFT.
  int data_size = data->size() / 2;
  double rms = 0.0;
  for (int t = 0; t < data_size; ++t) {
    const double amplitude = (*data)[2 * t];
    rms += amplitude * amplitude;
  }
  rms = sqrt(rms / data_size);
  if (verbose_)
    printf("rms: %0.4f\n", rms);

  if (rms < power_threshold_) {
    perror("The RMS level is too low.");
    return 0.0;
  }

  FFT(data);

  double confidence = 0.0, mean = 0.0, sigma = 0.0;

  for (int bin = (center_bin - half_window_size_);
       bin <= center_bin + half_window_size_;
       ++bin) {
    int index = bin - (center_bin - half_window_size_);
    bin_[index] = SquareAbs(
        (*data)[2 * bin], (*data)[2 * bin + 1]) / data->size();
    if (verbose_)
      printf("%e ", bin_[index]);
    confidence += bin_[index] * filter_[index];
    mean += bin_[index];
    sigma += bin_[index] * bin_[index];
  }
  if (verbose_)
    printf("\n");
  // Avoids divide by zero.
  if (std::abs(sigma) < 1e-9)
    return 0.0;
  const double power_ratio = bin_[half_window_size_] / mean;
  mean /= filter_.size();
  sigma = sqrt(sigma / filter_.size() - mean * mean);
  confidence /= (sigma * filter_.size());
  if (verbose_)
    printf("power: %0.4f, conf: %0.4f\n", power_ratio, confidence);
  return power_ratio * confidence;
}
