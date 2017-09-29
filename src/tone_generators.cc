// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/tone_generators.h"

#include <math.h>
#include <stdio.h>

#include <limits>

namespace {
  static const double kPi = 3.14159265358979323846264338327l;
  static const double kHalfPi = kPi / 2.0;
}

SineWaveGenerator::SineWaveGenerator(int sample_rate, double length_sec,
    int volume_gain)
    : cur_x_(0.0), cur_frame_(0), sample_rate_(sample_rate),
      length_sec(length_sec), frequency_(0.0), volume_gain_(volume_gain) {
  if (length_sec > 0)
    total_frame_ = length_sec * sample_rate;
  else
    total_frame_ = 0;
}

double SineWaveGenerator::Next() {
  cur_x_ += (kPi * 2 * frequency_) / sample_rate_;
  cur_frame_++;
  return sin(cur_x_) * volume_gain_ / 100.0;
}
void SineWaveGenerator::Reset(double frequency) {
  cur_x_ = 0;
  cur_frame_ = 0;
  frequency_ = frequency;
}

size_t SineWaveGenerator::GetFrames(SampleFormat format, int num_channels,
    const std::set<int> &active_channels, void *data, size_t buf_size) {

  int remain_frames = total_frame_ > 0
                      ? (static_cast<int>(total_frame_) - cur_frame_ - 1)
                      : std::numeric_limits<int>::max();
  int frame_required = buf_size / num_channels / format.bytes();
  int num_frames = std::min(frame_required, remain_frames);

  for (int i = 0; i < num_frames; ++i) {
    double sample = Next();
    for (int c = 0; c < num_channels; ++c) {
      if (active_channels.find(c) != active_channels.end())
        data = WriteSample(sample, format, data);
      else
        data = WriteSample(0.0f, format, data);
    }
  }
  return num_frames;
}

bool SineWaveGenerator::HasMoreFrames() const {
  if (total_frame_ > 0) {
    return cur_frame_ < total_frame_;
  }
  // Infinite.
  return true;
}

MultiToneGenerator::MultiToneGenerator(int sample_rate, double length_sec)
    : frames_generated_(0),
      frames_wanted_(length_sec * sample_rate),
      fade_frames_(0),  // Calculated below.
      sample_rate_(sample_rate),
      cur_vol_(1.0),
      start_vol_(1.0),
      inc_vol_(0.0) {

  // Use a fade of 2.5ms at both the start and end of a tone .
  const double kFadeTimeSec = 0.005;

  // Only fade if the fade won't take more than 1/2 the tone.
  if (length_sec > (kFadeTimeSec * 4)) {
    fade_frames_ = kFadeTimeSec * sample_rate;
  }

  frequencies_.clear();
  pthread_mutex_init(&param_mutex, NULL);
}

MultiToneGenerator::~MultiToneGenerator() {
  pthread_mutex_destroy(&param_mutex);
}

void MultiToneGenerator::SetVolumes(double start_vol, double end_vol) {
  pthread_mutex_lock(&param_mutex);
  cur_vol_ = start_vol_ = start_vol;
  inc_vol_ = (end_vol - start_vol) / frames_wanted_;
  pthread_mutex_unlock(&param_mutex);
}

void MultiToneGenerator::Reset(const std::vector<double> &frequencies,
                               bool reset_timer) {
  pthread_mutex_lock(&param_mutex);
  frequencies_ = frequencies;
  if (reset_timer) {
    frames_generated_ = 0;
    cur_vol_ = start_vol_;
  }
  pthread_mutex_unlock(&param_mutex);
}

void MultiToneGenerator::Reset(const double *frequency, int num_tones,
                               bool reset_timer) {
  pthread_mutex_lock(&param_mutex);
  frequencies_.resize(num_tones);
  for (int i = 0; i < num_tones; ++i) {
    frequencies_[i] = frequency[i];
  }
  if (reset_timer) {
    frames_generated_ = 0;
    cur_vol_ = start_vol_;
  }
  pthread_mutex_unlock(&param_mutex);
}

void MultiToneGenerator::Reset(double frequency, bool reset_timer) {
  pthread_mutex_lock(&param_mutex);
  frequencies_.resize(1);
  frequencies_[0] = frequency;
  if (reset_timer) {
    frames_generated_ = 0;
    cur_vol_ = start_vol_;
  }
  pthread_mutex_unlock(&param_mutex);
}

size_t MultiToneGenerator::GetFrames(SampleFormat format,
                                     int num_channels,
                                     const std::set<int> &active_channels,
                                     void *data,
                                     size_t buf_size) {
  const int kBytesPerFrame = num_channels * format.bytes();
  void *cur = data;
  int frames = buf_size / kBytesPerFrame;
  int frames_written;
  pthread_mutex_lock(&param_mutex);
  tone_wave_.resize(frequencies_.size(), SineWaveGenerator(sample_rate_));
  for (size_t f = 0; f < frequencies_.size(); ++f)
    tone_wave_[f].Reset(frequencies_[f]);

  for (frames_written = 0; frames_written < frames; ++frames_written) {
    if (!HasMoreFrames()) {
      break;
    }

    double frame_magnitude = 0;
    for (size_t f = 0; f < frequencies_.size(); ++f) {
      frame_magnitude += tone_wave_[f].Next();
    }
    frame_magnitude *= GetFadeMagnitude() * cur_vol_;
    if (frequencies_.size() > 1) {
      frame_magnitude /= static_cast<double>(frequencies_.size());
    }
    cur_vol_ += inc_vol_;
    for (int c = 0; c < num_channels; ++c) {
      if (active_channels.find(c) != active_channels.end()) {
        cur = WriteSample(frame_magnitude, format, cur);
      } else {
        // Silence the non-active channels.
        cur = WriteSample(0.0f, format, cur);
      }
    }

    ++frames_generated_;
  }
  pthread_mutex_unlock(&param_mutex);
  return frames_written * kBytesPerFrame;
}

bool MultiToneGenerator::HasMoreFrames() const {
  return frames_generated_ < frames_wanted_;
}

double MultiToneGenerator::GetFadeMagnitude() const {
  int frames_left = frames_wanted_ - frames_generated_;
  if (frames_generated_ < fade_frames_) {  // Fade in.
    return sin(kHalfPi * frames_generated_ / fade_frames_);
  } else if (frames_left < fade_frames_) {  // Fade out.
    return sin(kHalfPi * frames_left / fade_frames_);
  } else {
    return 1.0f;
  }
}

// A# minor harmoic scale is: A#, B# (C), C#, D#, E# (F), F#, G## (A).
const double ASharpMinorGenerator::kNoteFrequencies[] = {
  466.16, 523.25, 554.37, 622.25, 698.46, 739.99, 880.00, 932.33,
  932.33, 880.00, 739.99, 698.46, 622.25, 554.37, 523.25, 466.16,
};

ASharpMinorGenerator::ASharpMinorGenerator(int sample_rate,
                                           double tone_length_sec)
    : tone_generator_(sample_rate, tone_length_sec),
      cur_note_(0) {
  tone_generator_.Reset(kNoteFrequencies[cur_note_], true);
}

ASharpMinorGenerator::~ASharpMinorGenerator() {
}

void ASharpMinorGenerator::SetVolumes(double start_vol, double end_vol) {
  tone_generator_.SetVolumes(start_vol, end_vol);
}

void ASharpMinorGenerator::Reset() {
  cur_note_ = 0;
  tone_generator_.Reset(kNoteFrequencies[cur_note_], true);
}

size_t ASharpMinorGenerator::GetFrames(SampleFormat format,
                                       int num_channels,
                                       const std::set<int> &active_channels,
                                       void *data,
                                       size_t buf_size) {
  if (!HasMoreFrames()) {
    return 0;
  }

  // Go to next note if necessary.
  if (!tone_generator_.HasMoreFrames()) {
    tone_generator_.Reset(kNoteFrequencies[++cur_note_], true);
  }

  return tone_generator_.GetFrames(format,
                                   num_channels,
                                   active_channels,
                                   data,
                                   buf_size);
}

bool ASharpMinorGenerator::HasMoreFrames() const {
  return cur_note_ < kNumNotes - 1 || tone_generator_.HasMoreFrames();
}
