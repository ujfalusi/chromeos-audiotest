// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/connector.h"

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <iostream>

namespace autotest_client {
namespace audio {

void PlayClient::InitProcess(bool log_to_file) {
  int pipe_fd[2];

  bool fifo_mode = false;
  if (!fifo_name_.empty()) {  // use fifo
    mkfifo(fifo_name_.c_str(), 0600);
    fifo_mode = true;
  } else if (pipe(pipe_fd) < 0) {
    perror("Failed to create pipe for player program");
    exit(EXIT_FAILURE);
  }

  child_pid_ = fork();

  if (child_pid_ < 0) {
    perror("Failed to fork for player program");
  } else if (child_pid_ == 0) {  // child
    if (!fifo_mode) {
      dup2(pipe_fd[0], STDIN_FILENO);
      close(pipe_fd[1]);
    }

    // Writes log to file if set.
    if (log_to_file) {
      int file_err = open("recorder.err",
                          O_WRONLY | O_TRUNC | O_CREAT, S_IRWXU);
      assert(file_err > 0);
      dup2(file_err, STDERR_FILENO);
    }

    if (player_.Exec() < 0) {
      perror("Exec player");
      kill(getppid(), SIGKILL);
      exit(EXIT_FAILURE);
    }
  } else {  // parent
    if (fifo_mode) {
      play_fd_ = open(fifo_name_.c_str(), O_WRONLY);
    } else {
      play_fd_ = pipe_fd[1];
      close(pipe_fd[0]);
    }
  }
}

int PlayClient::Play(const vector< vector<double> > &block,
                     unsigned num_frames) {
  size_t buf_size = num_frames * num_channels_ * format_.bytes();
  char *buf = new char[buf_size];
  if (num_frames > max_num_frames_) {
    cerr << "Request value: " << num_frames
         << " > Maximum frames capability: " << max_num_frames_ << endl;
    num_frames = max_num_frames_;
  }
  void *cur = buf;
  for (unsigned f = 0; f < num_frames; ++f) {
    for (int ch = 0; ch < num_channels_; ++ch) {
      cur = WriteSample(block[ch][f], cur, format_);
    }
  }
  int res;
  int cur_write = buf_size;
  // Keep writing to pipe until error or finishing.
  while ((res = write(play_fd_, buf, cur_write)) < cur_write) {
    if (res < 0) {
      return res;
    }
    buf += res;
    cur_write -= res;
  }
  return num_frames;
}


void PlayClient::Terminate() {
  kill(child_pid_, SIGINT);
  close(play_fd_);
}

void RecordClient::InitProcess(bool log_to_file) {
  int pipe_fd[2];
  /* fifo OR pipe */
  bool fifo_mode = false;
  if (!fifo_name_.empty()) {  // use fifo
    mkfifo(fifo_name_.c_str(), 0600);
    fifo_mode = true;
  } else if (pipe(pipe_fd) < 0) {  // stdout
    perror("Failed to create pipe for recorder program");
    exit(EXIT_FAILURE);
  }

  child_pid_ = fork();

  if (child_pid_ < 0) {
    perror("Failed to fork for recorder");
  } else if (child_pid_ == 0) {  // child
    if (!fifo_mode) {
      dup2(pipe_fd[1], STDOUT_FILENO);
      close(pipe_fd[0]);
    }

    // Writes log to file if set.
    if (log_to_file) {
      int file_err = open("recorder.err",
                          O_WRONLY | O_TRUNC | O_CREAT, S_IRWXU);
      assert(file_err > 0);
      dup2(file_err, STDERR_FILENO);
    }

    if (recorder_.Exec() < 0) {
      perror("Failed to exec recorder");
      kill(getppid(), SIGKILL);
      exit(EXIT_FAILURE);
    }
  } else {  // parent
    if (fifo_mode) {
      record_fd_ = open(fifo_name_.c_str(), O_RDWR);
    } else {
      record_fd_ = pipe_fd[0];
      close(pipe_fd[1]);
    }
  }
}

int RecordClient::Record(vector< vector<double> > *sample_ptr,
                         unsigned num_frames) {
  size_t bufsize = num_frames * num_channels_ * format_.bytes();
  char *buf = new char[bufsize];
  int res = read(record_fd_, buf, bufsize);
  if (res <= 0)
    return res;

  FramesToSamples(buf, bufsize, num_channels_, sample_ptr, format_);
  delete [] buf;
  // Returns number of frames read.
  return res / (num_channels_ * format_.bytes());
}

void RecordClient::Terminate() {
  kill(child_pid_, SIGINT);
  close(record_fd_);
}

}  // namespace audio
}  // namespace autotest_client
