// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/frame_generator.h"

#include "include/util.h"

typedef std::unique_lock<std::mutex> ulock;

FrameGenerator::FrameGenerator(const AudioFunTestConfig &config)
    : terminate_test_(false),
      new_freq_(false),
      stop_play_tone_(false),
      fft_size_(config.fft_size),
      sample_rate_(config.sample_rate),
      tone_length_(config.tone_length_sec),
      num_channels_(config.num_speaker_channels),
      active_channels_(config.active_speaker_channels),
      format_(config.sample_format),
      wave_(config.sample_rate),
      num_fade_frames_(0),
      is_debug_(config.verbose) {
  frames_needed_ = sample_rate_ * tone_length_;
  frames_generated_ = 0;

  const double FadingLengthSec = 0.005;
  if (tone_length_ > FadingLengthSec * 4) {
    num_fade_frames_ = FadingLengthSec * sample_rate_;
  }
}

void FrameGenerator::Run(PlayClient *player) {
  double *buffer = new double[fft_size_];

  while (!terminate_test_) {
    // Wait for new frequency.
    ulock lock(change_state_mutex_);
    change_state_cond_.wait(lock, [=]{return new_freq_ || terminate_test_;});
    new_freq_ = false;
    lock.unlock();

    if (terminate_test_) break;

    if (is_debug_) printf("[Generator] Got new frequency to play.\n");

    while (!stop_play_tone_ && HasMoreFrames()) {
      int written = GetFrames(buffer, fft_size_);
      std::vector< std::vector<double> > chn_buf;
      for (int ch = 0; ch < num_channels_; ++ch) {
        if (active_channels_.count(ch) > 0)
          chn_buf.push_back(std::vector<double>(buffer, buffer + fft_size_));
        else
          chn_buf.push_back(std::vector<double>(fft_size_, 0.0));
      }
      if (player->Play(chn_buf, written) < 0) {
        printf("Play frames error.\n");
        assert(false);
      }
    }

    stop_play_tone_ = false;
    if (is_debug_)
      printf("[Player] Done play the tone.\n");
  }

  delete [] buffer;
}

void FrameGenerator::Stop() {
  // Close the play thread.
  ulock lock(change_state_mutex_);
  terminate_test_ = true;
  change_state_cond_.notify_all();
  lock.unlock();


  generating_thread_.join();
}

void FrameGenerator::SetFrequency(double freq) {
  wave_.Reset(freq);
  frames_generated_ = 0;

  // Notify generating thread.
  ulock lock(change_state_mutex_);
  new_freq_ = true;
  change_state_cond_.notify_one();
  lock.unlock();
}

unsigned FrameGenerator::GetFrames(double *buf, unsigned num_frames) {
  unsigned frame_written;
  for (frame_written = 0; frame_written < num_frames; ++frame_written) {
    if (!HasMoreFrames()) {
      break;
    }
    buf[frame_written] = GetSample();
    ++frames_generated_;
  }
  return frame_written;
}

bool FrameGenerator::HasMoreFrames() const {
  return frames_generated_ < frames_needed_;
}

double FrameGenerator::GetSample() {
  double magn = 0.0;
  // Determined by frequency.
  magn += wave_.GetNext();

  // Effected by fading & volume.
  magn *= FadingMagnitude();
  return magn;
}

double FrameGenerator::FadingMagnitude() const {
  int frame_border = std::min(
      frames_generated_, (frames_needed_ - frames_generated_));

  if (frame_border < num_fade_frames_) {
    return sin(M_PI / 2 * frame_border / num_fade_frames_);
  }
  return 1.0;
}
