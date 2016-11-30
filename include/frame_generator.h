// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_FRAME_GENERATOR_H_
#define INCLUDE_FRAME_GENERATOR_H_

#include <math.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <set>
#include <thread>

#include "include/common.h"
#include "include/connector.h"

// Sine wave is a object recording
// the current amplitude of the specific frequency
// with respect to sample rate.
class SineWave {
 public:
  explicit SineWave(int sample_rate, double freq = 0.0)
      : frequency_(freq),
        x_(0.0),
        sample_rate_(sample_rate) {}

  // Resets the frequency of wave.
  void Reset(double freq) {
    frequency_ = freq;
    x_ = 0.0;
  }

  // Get next sample.
  double GetNext() {
    x_ += (2 * M_PI * frequency_) / static_cast<double>(sample_rate_);
    return sin(x_);
  }


 private:
  double frequency_;
  double x_;
  int sample_rate_;
};

// Control the generating thread
// which generates frames with respect to specified frequency
// and write to player process.
class FrameGenerator {
 public:
  explicit FrameGenerator(const AudioFunTestConfig &config);

  // Start generating thread and play.
  inline void PlayTo(PlayClient *player) {
    generating_thread_ = std::thread(&FrameGenerator::Run, this, player);
  }

  // Reset the frequencies to be generated.
  void SetFrequency(double freq);

  // Tell player thread to stop playing current frequency.
  inline void SetStopPlayTone() {
    stop_play_tone_ = true;
  }

  void Stop();

 private:
  // The function of generating thread.
  // Keep direct frames to player whenever frequency is available.
  void Run(PlayClient *player);

  // Generates frames and put into the buffer.
  // Returns number of frames generated.
  unsigned GetFrames(double *buf, unsigned num_frames);

  // Returns true if there is more frames to be generated.
  bool HasMoreFrames() const;

  // Get magnitude for single frame by frequency, fading, etc.
  double GetSample();

  // Calculate the fading-in proportion
  // so it won't hurt the speaker by directly play with huge volume.
  double FadingMagnitude() const;

  std::thread generating_thread_;

  // Two states when playing
  // use change_state_mutex & change_state_cond to update.
  bool terminate_test_;
  bool new_freq_;

  // Use this to avoid additional tones played.
  std::atomic<bool> stop_play_tone_;

  std::mutex change_state_mutex_;
  std::condition_variable change_state_cond_;

  // Frame records.
  int frames_needed_;
  int frames_generated_;

  // Static data for tone generator.
  int fft_size_;
  int sample_rate_;
  double tone_length_;
  int num_channels_;
  std::set<int> active_channels_;

  SampleFormat format_;

  SineWave wave_;

  int num_fade_frames_;

  bool is_debug_;
};

#endif  // INCLUDE_FRAME_GENERATOR_H_
