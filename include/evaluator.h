// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_EVALUATOR_H_
#define INCLUDE_EVALUATOR_H_

#include <math.h>

#include <set>
#include <vector>

#include "include/common.h"
#include "include/connector.h"

// Evaluates the frames coming from recorder process
// and keep tracking of accumulated evaluating confidence of correctness.
class Evaluator {
 public:
  // bin_range: The range of bins neighboring to expected frequency bin
  //            going to perform zero-mean & unit variance normalization.
  explicit Evaluator(const AudioFunTestConfig &, int bin_range = 3);

  // Evaluates the sampled frames.
  int Evaluate(RecordClient *recorder,
               int center_bin,
               std::vector<bool> *single_output);

 private:
  // One dimension fast fourier transform
  // using Danielson-Lanczos lemma.
  void FFT(std::vector<double> *data_ptr) const;

  // Returns the matched filter confidence the single channel.
  double EstimateChannel(std::vector<double> *cell_ptr, int center_bin);

  std::vector<double> filter_;
  int bin_range_;
  int num_channels_;
  int buffer_size_;
  SampleFormat format_;
  int sample_rate_;
  std::vector<double> bin_;

  double confidence_threshold_;
  int max_trial_;

  bool is_debug_;
};

#endif  // INCLUDE_EVALUATOR_H_
