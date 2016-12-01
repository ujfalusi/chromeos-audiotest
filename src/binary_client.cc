// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/binary_client.h"

#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

// Fork, exec child process and set its stdin / stdout fd.
//
// Args:
//   stdin_fd: the fd to be set as child's stdin. Ignored if <= 0.
//   stdout_fd: the fd to be set as child's stdout. Ignored if <= 0.
// Returns:
//   The child's pid.
int StartProcess(const std::string &cmd, int stdin_fd, int stdout_fd) {
  // Parse command into execvp accepted format.
  std::vector<char *> args;
  char *token;
  char *buf = new char[cmd.size() + 1];
  strncpy(buf, cmd.c_str(), cmd.size() + 1);
  for (char *ptr = buf, *saveptr; ; ptr = NULL) {
    token = strtok_r(ptr, " ", &saveptr);
    args.push_back(token);
    if (token == NULL) break;
  }

  // fork process;
  int child_pid = fork();

  if (child_pid < 0) {
    perror("Failed to fork for player program");
    exit(EXIT_FAILURE);
  } else if (child_pid == 0) {  // child
    if (stdin_fd > 0) {
      dup2(stdin_fd, STDIN_FILENO);
    }
    if (stdout_fd > 0) {
      dup2(stdout_fd, STDOUT_FILENO);
    }
    int res = execvp(args[0], (char * const *) &args[0]);
    if (res < 0) {
      perror("Failed to exec client");
      kill(getppid(), SIGKILL);
      exit(EXIT_FAILURE);
    }
  }
  delete [] buf;

  // parent
  return child_pid;
}

// Flags used for `CreateFIFO`.
const bool FIFO_IN = true;
const bool FIFO_OUT = false;

// Creates a FIFO to communicate with other process. If `fifo_name` is
// empty, then an unnamed pipe is created, and `other_side_fd` is set to the
// fd of the other end. Otherwise, a named pipe is created and opened, and
// `other_side_fd` would be set to -1.
//
// Returns the fd of the channel.
int CreateFIFO(bool direction, const std::string &fifo_name,
               int *other_side_fd) {
  int fd = -1;
  *other_side_fd = -1;
  if (!fifo_name.empty()) {  // Create named pipe.
    const char *name = fifo_name.c_str();
    mkfifo(name, 0600);
    fd = open(name, direction == FIFO_IN ? O_RDONLY : O_WRONLY);
    if (fd < 0) {
      perror("Failed to open fifo.");
      exit(EXIT_FAILURE);
    }
  } else {
    int pipe_fd[2];
    if (pipe(pipe_fd) < 0) {
      perror("Failed to create pipe.");
      exit(EXIT_FAILURE);
    }
    fd = pipe_fd[direction == FIFO_IN ? 0 : 1];
    *other_side_fd = pipe_fd[direction == FIFO_IN ? 1 : 0];
  }

  // Keep pipe buffer small to reduce latency.
  if (fcntl(fd, F_SETPIPE_SZ, 1) <= 0) {
    perror("Failed to set pipe buffer size.");
    exit(EXIT_FAILURE);
  }
  return fd;
}

}  // namespace

void PlayClient::Start() {
  int other_side_fd;
  play_fd_ = CreateFIFO(FIFO_OUT, fifo_name_, &other_side_fd);
  child_pid_ = StartProcess(command_, other_side_fd, -1);
}

void PlayClient::Terminate() {
  close(play_fd_);
  // TODO(shunhsingou) terminate the process gracefully.
  kill(child_pid_, SIGKILL);
}

void PlayClient::Play(const void *buffer, size_t size) {
  int res;
  int byte_to_write = size;
  const uint8_t *ptr = static_cast<const uint8_t *>(buffer);
  // Keep writing to pipe until error or finishing.
  while ((res = write(play_fd_, ptr, byte_to_write)) < byte_to_write) {
    if (res < 0) {
      perror("Failed to write to player.");
      exit(EXIT_FAILURE);
    }
    ptr += res;
    byte_to_write -= res;
  }
}

void RecordClient::Start() {
  int other_side_fd;
  record_fd_ = CreateFIFO(FIFO_IN, fifo_name_, &other_side_fd);
  child_pid_ = StartProcess(command_, -1, other_side_fd);
}

void RecordClient::Record(void *buffer, size_t size) {
  int res;
  int byte_to_read = size;

  uint8_t *ptr = static_cast<uint8_t *>(buffer);

  while ((res = read(record_fd_, ptr, byte_to_read)) < byte_to_read) {
    if (res < 0) {
      fprintf(stderr, "Retrieve recorded data error.\n");
      exit(EXIT_FAILURE);
    }
    ptr += res;
    byte_to_read -= res;
  }
}

void RecordClient::Terminate() {
  close(record_fd_);
  // TODO(shunhsingou) terminate the process gracefully.
  kill(child_pid_, SIGKILL);
}
