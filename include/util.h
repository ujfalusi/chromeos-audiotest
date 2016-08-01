// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUDIOTEST_UTIL_H_
#define AUDIOTEST_UTIL_H_

#include <limits>
#include <iostream>
#include <vector>

#include "common.h"

#define BYTE(X, i) ((reinterpret_cast<unsigned char *>(&X))[i])
using std::vector;

namespace autotest_client {
namespace audio {

// Returns the square of absolute value of complex number.
inline double SquareAbs(const double real, const double imaginary) {
  return real * real + imaginary * imaginary;
}

// Gets the number of frames needed, w.r.t. bufsize.
inline unsigned BufsizeToFrames(size_t bufsize,
                                SampleFormat format,
                                int num_channels) {
  return bufsize / (format.bytes() * num_channels);
}

// Writes a sample to the char buffer with the given data type.
// Sample should be a value between 1.0 ~ -1.0.
// Returns the next position of the buffer.
template <typename T>
void *WriteSample(double sample, void *buf) {
  // Handles unsigned type.
  if (std::is_unsigned<T>::value) {
    sample += 1.0;
    sample /= 2.0;
  }

  T* sample_data = reinterpret_cast<T*>(buf);
  *sample_data = sample * std::numeric_limits<T>::max();
  return sample_data + 1;
}

// Detects big endian at run time.
inline bool IsBigEndian() {
  union {
    uint32_t i;
    char c[4];
  } combine = {0x01020304};
  return combine.c[0] == 1;
}

// Writes a sample to the char buffer with the given data type.
// Sample should be a value between 1.0 ~ -1.0.
// Returns the next position of the buffer.
inline void *WriteSample(double sample, void *buf, SampleFormat format) {
  if (format.type() == SampleFormat::kPcmU8) {
    return WriteSample<unsigned char>(sample, buf);

  } else if (format.type() == SampleFormat::kPcmS16) {
    return WriteSample<int16_t>(sample, buf);

  } else if (format.type() == SampleFormat::kPcmS24) {
    unsigned char* sample_data = reinterpret_cast<unsigned char*>(buf);
    int32_t value = sample * (1 << 23);  // 1 << 23 24-bit signed max().
    if (IsBigEndian()) {
      for (int i = 0; i < 3; i++) {
        sample_data[i] = BYTE(value, i+1);
      }
    } else {
      for (int i = 0; i < 3; i++) {
        sample_data[i] = BYTE(value, i);
      }
    }
    return sample_data + 3;

  } else if (format.type() == SampleFormat::kPcmS32) {
    return WriteSample<int32_t>(sample, buf);
  }

  // Returns NULL, which should crash the caller.
  std::cerr << "Unknown format when doing conversion." << std::endl;
  assert(false);
  return NULL;
}

// Reads from raw data and returns a sample between 1.0 ~ -1.0.
template<typename T>
inline double ReadSample(T data) {
  double val = static_cast<double>(data) / std::numeric_limits<T>::max();
  if (std::numeric_limits<T>::min() == 0) {
    val = val * 2.0 - 1.0;
  }
  return val;
}

// Reads from raw data and converts them to samples with double type.
template<typename T>
inline void FramesToSamples(void* data,
                            int num_frames,
                            int num_channels,
                            vector< vector<double> > *sample_ptr) {
  vector< vector<double> >& mag = *sample_ptr;
  T *ptr = static_cast<T*>(data);

  for (int n = 0; n < num_frames; n++) {
    for (int c = 0; c < num_channels; c++) {
      mag[c][n] = ReadSample<T>(*(ptr++));
    }
  }
}

// Reads from raw data and converts them to samples with double type.
inline void FramesToSamples(void* data,
                            int bufsize,
                            int num_channels,
                            vector< vector<double> > *sample_ptr,
                            SampleFormat format) {
  unsigned num_frames = bufsize / (format.bytes() * num_channels);

  if (format.type() == SampleFormat::kPcmU8) {
    FramesToSamples<unsigned char>
        (data, num_frames, num_channels, sample_ptr);

  } else if (format.type() == SampleFormat::kPcmS16) {
    FramesToSamples<int16_t>
        (data, num_frames, num_channels, sample_ptr);

  } else if (format.type() == SampleFormat::kPcmS24) {
    auto& mag = *sample_ptr;
    unsigned char *ptr = static_cast<unsigned char*>(data);

    for (unsigned n = 0; n < num_frames; n++) {
      for (int c = 0; c < num_channels; c++) {
        int32_t value = 0;
        if (IsBigEndian()) {
          for (int i = 2; i >= 0; i--)
            value |= (static_cast<int>(*(ptr++))) << (8 * i);
        } else {
          for (int i = 0; i < 3; i++)
            value |= (static_cast<int>(*(ptr++))) << (8 * i);
        }
        mag[c][n] = static_cast<double>(value) / (1 << 23);
      }
    }
  } else if (format.type() == SampleFormat::kPcmS32) {
    FramesToSamples<int32_t>
        (data, num_frames, num_channels, sample_ptr);
  }
}

}  // namespace audio
}  // namespace autotest_client

#endif  // AUDIOTEST_UTIL_H_
