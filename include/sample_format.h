// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_SAMPLE_FORMAT_H_
#define INCLUDE_SAMPLE_FORMAT_H_

#include <stddef.h>

#include <vector>

class SampleFormat {
 public:
  enum Type {
    kPcmU8,
    kPcmS16,
    kPcmS24,
    kPcmS32,
    kPcmInvalid,
  };

  SampleFormat();
  explicit SampleFormat(Type type);
  void set_type(Type type);
  Type type() const;
  const char* to_string() const;
  size_t bytes() const;
  inline bool operator==(const SampleFormat& format) const;

 private:
  Type type_;
};

// Writes sample into the buffer with the specific format.
// Returns the next position after writing.
void* WriteSample(double sample, SampleFormat format, void* buf);

void* ReadSample(SampleFormat format, void* data, double* sample);

// Unpack the data read from recorder.
// The input data is a byte array with interlaced data (usually the raw data
// obtained from recorder.).
//
// This function do the following convertion:
// 1. Deinterlace data into each channel.
// 2. Normalize each sample into -1.0 ~ 1.0.
//
// Returns number of frames processed.
int Unpack(void* data,
           size_t data_size,
           SampleFormat format,
           int num_channels,
           std::vector<std::vector<double>>* output);

#endif  // INCLUDE_SAMPLE_FORMAT_H_
