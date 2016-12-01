// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_EVALUATOR_H_
#define INCLUDE_EVALUATOR_H_

#include <vector>

#include "include/binary_client.h"
#include "include/common.h"
#include "include/sample_format.h"

class Evaluator {
 public:
  explicit Evaluator(const AudioFunTestConfig &);
  ~Evaluator();

  // Evaluates the recorded wave and compared with the expected bin.
  // Saves the result in the vector that indicates the successness of each mic
  // channels.
  void Evaluate(int center_bin,
                RecordClient *recorder,
                std::vector<bool> *result);

 private:
  // Returns the matched filter confidence the single channel.
  double EstimateChannel(std::vector<double> *data, int center_bin);

  std::vector<double> filter_;
  int half_window_size_;
  int num_channels_;
  SampleFormat format_;
  int sample_rate_;
  std::vector<double> bin_;
  uint8_t* buffer_;
  size_t buf_size_;

  double confidence_threshold_;
  int max_trial_;

  bool verbose_;
};

#endif  // INCLUDE_EVALUATOR_H_
