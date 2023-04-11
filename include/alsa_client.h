// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_ALSA_CLIENT_H_
#define INCLUDE_ALSA_CLIENT_H_

#include <alsa/asoundlib.h>
#include <stdio.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "include/common.h"
#include "include/sample_format.h"

// Alsa API forward declares.
struct _snd_pcm;

class ToneGenerator;

_snd_pcm_format SampleFormatToAlsaFormat(SampleFormat format);

/* Calculate number of bytes per frame given format and channels */
int SampleFormatToFrameBytes(SampleFormat format, int channels);

/*
 * Maintain a circular buffer of type T, which has a vector of <count> cells.
 * Each cell contains a vector of T with size <size>. Whenever LockCellToWrite()
 * is called, the corresponding cell is locked and the pointer to T is returned.
 * After the data was written to the cell, UnlockCellToWrite() should be called
 * which will release the mutex and signal pthread_cond_t so any blocking
 * LockCellToRead() can proceed.
 * At anytime the internal <write_index_> is strictly less than <read_index_>
 * indicating there is data to read. Otherwise LockCellToRead() will block until
 * write_index_ is increased and UnlockCellToWrite() is called.
 */
template <typename T>
class CircularBuffer {
 public:
  CircularBuffer(int count, int size)
      : buffer_count_(count),
        buffer_size_(size),
        write_index_(0),
        read_index_(0) {
    cell_.resize(count);
    data_.resize(count * size);
    for (int i = 0; i < count; ++i) {
      cell_[i] = data_.data() + i * size;
    }
    mutexes_.resize(count);
    has_data_.resize(count);
    for (int i = 0; i < count; ++i) {
      pthread_mutex_init(&mutexes_[i], NULL);
      pthread_cond_init(&has_data_[i], NULL);
    }
  }

  ~CircularBuffer() {}

  /* Lock mutex, return cell pointer
   * MUST call UnlockCellToWrite(); after work is done.
   */
  T* LockCellToWrite(int* index = NULL) {
    if (index)
      *index = write_index_;
    pthread_mutex_lock(&mutexes_[write_index_]);
    return cell_[write_index_];
  }

  /* Unlock mutex, release mutex and signal for has_data_ */
  void UnlockCellToWrite() {
    int last = write_index_;
    write_index_ = (write_index_ + 1) % buffer_count_;
    pthread_cond_signal(&has_data_[last]);
    pthread_mutex_unlock(&mutexes_[last]);
  }

  /* Lock mutex and return cell pointer.
   * MUST call UnlockCellToRead(); after work is done.
   */
  T* LockCellToRead(int* index = NULL) {
    if (index)
      *index = read_index_;
    pthread_mutex_lock(&mutexes_[read_index_]);
    while (read_index_ == write_index_) {
      pthread_cond_wait(&has_data_[read_index_], &mutexes_[read_index_]);
    }
    return cell_[read_index_];
  }

  /* Unlock mutex and increment read_index_. */
  void UnlockCellToRead() {
    int last = read_index_;
    read_index_ = (read_index_ + 1) % buffer_count_;
    pthread_mutex_unlock(&mutexes_[last]);
  }

  void Print(FILE* fp) {
    fprintf(fp, "    buffer_count_ = %d\n", buffer_count_);
    fprintf(fp, "    buffer_size_ = %d\n", buffer_size_);
    fprintf(fp, "    write_index_ = %d\n", write_index_);
    fprintf(fp, "    read_index_ = %d\n", read_index_);
  }
  int Count() const { return buffer_count_; }
  int Size() const { return buffer_size_; }

 private:
  int buffer_count_;
  int buffer_size_;
  int write_index_, read_index_;
  std::vector<T*> cell_;
  std::vector<T> data_;
  std::vector<pthread_mutex_t> mutexes_;
  std::vector<pthread_cond_t> has_data_;
};

inline size_t NumFrames(const CircularBuffer<char>& buffers,
                        SampleFormat format,
                        int num_channels) {
  return buffers.Size() / SampleFormatToFrameBytes(format, num_channels);
}

class AlsaPlaybackClient {
 public:
  enum State {
    kCreated,
    kFailed,
    kTerminated,
    kReady,
    kComplete,
  };

  class PlaybackParam {
    friend class AlsaPlaybackClient;
    PlaybackParam() : chunk_(nullptr), num_frames_(0), frame_bytes_(0) {}
    int Init(_snd_pcm* handle, SampleFormat format, int num_channels);
    void Print(FILE* fp);

    std::unique_ptr<char[]> chunk_;
    size_t num_frames_;
    int frame_bytes_;
  };

  AlsaPlaybackClient();
  explicit AlsaPlaybackClient(const std::string& playback_device);
  virtual ~AlsaPlaybackClient();

  virtual void Print(FILE* fp);
  void SetPlayObj(ToneGenerator* gen) { generator_ = gen; }
  ToneGenerator* PlayObj() { return generator_; }

  virtual bool Init(int sample_rate,
                    SampleFormat format,
                    int num_channels,
                    std::set<int>* act_chs,
                    int period_size = 0);
  virtual void PlayTones();
  virtual void Play(std::shared_ptr<CircularBuffer<char>> buffers);

  // Trivial accessors/mutators.
  virtual void set_state(State state) { state_ = state; }
  virtual State state() const { return state_; }
  virtual int last_error() const { return last_error_; }
  virtual int SampRate() const { return sample_rate_; }
  virtual int NumChannel() const { return num_channels_; }
  virtual SampleFormat Format() const { return format_; }
  virtual std::set<int>* ActiveChannels() const { return active_channels_; }

 private:
  static const unsigned kDefaultLatencyMs = 50;

  _snd_pcm* pcm_out_handle_;
  int sample_rate_;
  int num_channels_;
  SampleFormat format_;
  unsigned int latency_ms_;
  PlaybackParam pb_param_;
  std::set<int>* active_channels_;

  // Our abstracted version of the connection state.
  State state_;

  // The last error reported by Alsa. Useful for debugging.
  int last_error_;

  // The playback device to open.
  std::string playback_device_;

  // snd_pcm_set_params() argument when PlayThreadEntry() calls PlayTones()
  ToneGenerator* generator_;
};

class AlsaCaptureClient {
 public:
  enum State {
    kCreated,
    kFailed,
    kTerminated,
    kReady,
    kComplete,
  };

  AlsaCaptureClient();
  explicit AlsaCaptureClient(const std::string& capture_device);
  virtual ~AlsaCaptureClient();

  virtual bool Init(int sample_rate,
                    SampleFormat format,
                    int num_channels,
                    int buffer_count,
                    int period_size = 0);
  virtual void Print(FILE* fp);

  virtual int Capture();

  // Trivial accessors/mutators.
  virtual snd_pcm_hw_params_t* get_hw_params() const { return hwparams_; }
  virtual void set_state(State state) { state_ = state; }
  virtual State state() const { return state_; }
  virtual int last_error() const { return last_error_; }
  virtual int SampRate() const { return sample_rate_; }
  virtual int NumChannel() const { return num_channels_; }
  virtual SampleFormat Format() const { return format_; }
  virtual std::shared_ptr<CircularBuffer<char>> Buffer() const {
    return circular_buffer_;
  }

 private:
  static const unsigned kDefaultLatencyMs = 50;

  _snd_pcm* pcm_capture_handle_;
  snd_pcm_hw_params_t* hwparams_;
  unsigned int sample_rate_;
  int num_channels_;
  SampleFormat format_;
  unsigned int latency_ms_;

  // Our abstracted version of the connection state.
  State state_;

  // The last error reported by Alsa. Useful for debugging.
  int last_error_;

  // The playback device to open.
  std::string capture_device_;

  // Circular buffer to write captured data
  std::shared_ptr<CircularBuffer<char>> circular_buffer_;
};

#endif  // INCLUDE_ALSA_CLIENT_H_
