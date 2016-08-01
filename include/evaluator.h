// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUDIOTEST_EVALUATOR_H_
#define AUDIOTEST_EVALUATOR_H_

#include <cmath>
#include <iostream>
#include <set>
#include <vector>

#include "connector.h"
#include "param_config.h"

using std::vector;
using std::set;

namespace autotest_client {
namespace audio {

// Evaluates the frames coming from recorder process
// and keep tracking of accumulated evaluating confidence of correctness.
class Evaluator {
 public:
  // bin_range: The range of bins neighboring to expected frequency bin
  //            going to perform zero-mean & unit variance normalization.
  explicit Evaluator(const ParamConfig&, int bin_range = 3);

  // Evaluates the sampled frames.
  int Evaluate(RecordClient *recorder,
               int center_bin,
               vector<bool> *single_output);

 private:
  // One dimension fast fourier transform
  // using Danielson-Lanczos lemma.
  void FFT(vector<double> *data_ptr) const;

  // Returns the matched filter confidence the single channel.
  double EstimateChannel(vector<double> *cell_ptr, int center_bin);

  vector<double> filter_;
  int bin_range_;
  const set<int>& active_channels_;
  int num_channels_;
  size_t buffer_size_;
  SampleFormat format_;
  int sample_rate_;
  vector<double> bin_;

  double pass_threshold_;
  int max_trial_;

  bool is_debug_;
};

}  // namespace audio
}  // namespace autotest_client

#endif  // AUDIOTEST_EVALUATOR_H_
