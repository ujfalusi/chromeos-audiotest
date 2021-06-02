// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/sample_format.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <limits>

namespace {

template <typename T>
void *WriteSample(double sample, void *data) {
  // Handle unsigned.
  if (std::numeric_limits<T>::min() == 0) {
    sample = (sample + 1.0) / 2.0;
  }

  *(reinterpret_cast<T *>(data)) = sample * std::numeric_limits<T>::max();
  return reinterpret_cast<T *>(data) + 1;
}

// Detects big endian at run time.
bool *gIsBigEndian = 0;
bool IsBigEndian() {
  if (!gIsBigEndian) {
    gIsBigEndian = new bool;
    union {
      uint32_t i;
      char c[4];
    } combine = {0x01020304};
    *gIsBigEndian = combine.c[0] == 1;
  }
  return *gIsBigEndian;
}

#define BYTE(X, i) (reinterpret_cast<uint8_t *>(&(X))[(i)])

// Reads from raw data and returns a sample between 1.0 ~ -1.0.
template<typename T>
double ReadSample(T data) {
  double val = static_cast<double>(data) / std::numeric_limits<T>::max();
  if (std::numeric_limits<T>::min() == 0) {
    val = val * 2.0 - 1.0;
  }
  return val;
}

}  // namespace

SampleFormat::SampleFormat(): type_(kPcmInvalid) {}
SampleFormat::SampleFormat(Type type): type_(type) {}

void SampleFormat::set_type(Type type) {
  type_ = type;
}
SampleFormat::Type SampleFormat::type() const {
  return type_;
}

const char *SampleFormat::to_string() const {
  switch (type_) {
    case kPcmU8:
      return "u8";
    case kPcmS16:
      return "s16";
    case kPcmS24:
      return "s24";
    case kPcmS32:
      return "s32";
    default:
      return "INVALID";
  }
}

size_t SampleFormat::bytes() const {
  switch (type_) {
    case kPcmU8:
      return 1;
    case kPcmS16:
      return 2;
    case kPcmS24:
      return 3;
    case kPcmS32:
      return 4;
    default:
      return -1;
  }
}

bool SampleFormat::operator==(const SampleFormat &format) const {
  return type_ == format.type();
}

void *WriteSample(double sample, SampleFormat format, void *buf) {
  if (format.type() == SampleFormat::kPcmU8) {
    return WriteSample<uint8_t>(sample, buf);
  } else if (format.type() == SampleFormat::kPcmS16) {
    return WriteSample<int16_t>(sample, buf);
  } else if (format.type() == SampleFormat::kPcmS24) {
    uint8_t *sample_data = reinterpret_cast<uint8_t *>(buf);
    int32_t value = sample * (1 << 23);  // 1 << 23 24-bit singed max().
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
  assert(false);
  return NULL;
}

void *ReadSample(SampleFormat format, void *data, double *sample) {
  if (format.type() == SampleFormat::kPcmU8) {
    *sample = ReadSample<>(*reinterpret_cast<uint8_t *>(data));
    return reinterpret_cast<uint8_t *>(data) + 1;
  } else if (format.type() == SampleFormat::kPcmS16) {
    *sample = ReadSample<>(*reinterpret_cast<int16_t*>(data));
    return reinterpret_cast<int16_t*>(data) + 1;
  } else if (format.type() == SampleFormat::kPcmS24) {
    int32_t value = 0;
    uint8_t *ptr = static_cast<uint8_t *>(data);
    if (IsBigEndian()) {
      for (int i = 2; i >= 0; i--)
        value |= (static_cast<int>(*(ptr++))) << (8 * i);
    } else {
      for (int i = 0; i < 3; i++)
        value |= (static_cast<int>(*(ptr++))) << (8 * i);
    }
    *sample = static_cast<double>(value) / (1 << 23);
    return ptr + 3;
  } else if (format.type() == SampleFormat::kPcmS32) {
    *sample = ReadSample<>(*reinterpret_cast<int32_t*>(data));
    return reinterpret_cast<int32_t*>(data) + 1;
  }
  assert(false);
  return NULL;
}

int Unpack(void *data, size_t data_size,
           SampleFormat format, int num_channels,
           std::vector<std::vector<double> > *output) {
  int num_frames = data_size / format.bytes() / num_channels;
  output->clear();
  output->resize(num_channels, std::vector<double>(num_frames));
  for (int i = 0; i < num_frames; ++i) {
    for (int c = 0; c < num_channels; ++c) {
      data = ReadSample(format, data, &(*output)[c][i]);
    }
  }
  return num_frames;
}
