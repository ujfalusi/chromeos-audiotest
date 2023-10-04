// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "dolphin.h"

#include <alsa/asoundlib.h>
#include <stdint.h>
#include <termios.h>

int send_teensy_capture_start(struct dolphin* teensy_dev) {
  if (teensy_dev) {
    int res;
    const uint8_t msg = 'c';
    uint8_t reply_msg;
    res = write(teensy_dev->serial_fd, &msg, 1);
    if (res != 1) {
      fprintf(stderr, "Fail to send message to Teensy %d\n", res);
      return res;
    }
    res = read(teensy_dev->serial_fd, &reply_msg, 1);
    if (res != 1) {
      fprintf(stderr, "Fail to confirm Teensy capture started %d\n", res);
      return res;
    }
    fprintf(stderr, "Teensy capture start\n");
  } else {
    return -EINVAL;
  }
  return 0;
}

int get_teensy_capture_result(struct dolphin* teensy_dev) {
  if (teensy_dev) {
    int res;
    char buf[100];
    res = read(teensy_dev->serial_fd, buf, 100);
    if (res <= 0) {
      fprintf(stderr, "Fail to read latency\n");
      return res;
    } else {
      printf("latency: %s\n", buf);
    }
  } else {
    return -EINVAL;
  }
  return 0;
}

int setup_tty(int fd) {
  struct termios config;
  int res;

  //
  // Get the current configuration of the serial interface
  //
  res = tcgetattr(fd, &config);
  if (res < 0) {
    printf("Error in %d", __LINE__);
    return res;
  }

  //
  // Input flags - Turn off input processing
  //
  // convert break to null byte, no CR to NL translation,
  // no NL to CR translation, don't mark parity errors or breaks
  // no input parity check, don't strip high bit off,
  // no XON/XOFF software flow control
  //
  config.c_iflag &=
      ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);

  //
  // Output flags - Turn off output processing
  //
  // no CR to NL translation, no NL to CR-NL translation,
  // no NL to CR translation, no column 0 CR suppression,
  // no Ctrl-D suppression, no fill characters, no case mapping,
  // no local output processing
  //
  // config.c_oflag &= ~(OCRNL | ONLCR | ONLRET |
  //                     ONOCR | ONOEOT| OFILL | OLCUC | OPOST);
  config.c_oflag = 0;

  //
  // No line processing
  //
  // echo off, echo newline off, canonical mode off,
  // extended input processing off, signal chars off
  //
  config.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);

  //
  // Turn off character processing
  //
  // clear current char size mask, no parity checking,
  // no output processing, force 8 bit input
  //
  config.c_cflag &= ~(CSIZE | PARENB);
  config.c_cflag |= CS8;

  //
  // One input byte is enough to return from read()
  // Inter-character timer off
  //
  config.c_cc[VMIN] = 1;
  config.c_cc[VTIME] = 0;

  //
  // Communication speed (simple version, using the predefined
  // constants)
  //
  if (cfsetispeed(&config, B9600) < 0 || cfsetospeed(&config, B9600) < 0) {
    printf("Error in %d", __LINE__);
    return -1;
  };

  //
  // Finally, apply the configuration
  //
  res = tcsetattr(fd, TCSAFLUSH, &config);
  if (res < 0) {
    printf("Error in %d", __LINE__);
    return res;
  }

  return 0;
}

const char* serial_path = "/dev/ttyACM0";

int dolphin_ping_serial(struct dolphin* d) {
  uint8_t msg = 'l', reply_msg;
  int res;

  res = write(d->serial_fd, &msg, 1);
  if (res != 1) {
    goto error;
  }

  res = read(d->serial_fd, &reply_msg, 1);
  if (res != 1) {
    goto error;
  }

  if (reply_msg != 'l') {
    fprintf(stderr, "Fail to obtain correct response from Teensy\n");
    return -ENOMSG;
  }

  return 0;

error:
  fprintf(stderr, "Fail to ping serial\n");
  return res;
}

int get_serial_port(const char* serial_path) {
  int fd, res;
  fd = open(serial_path, O_RDWR);
  if (fd < 0) {
    return fd;
  }
  if ((res = setup_tty(fd)) < 0) {
    return res;
  }
  return fd;
}

struct dolphin* dolphin_create(const char* serial_path) {
  struct dolphin* d;
  int fd = get_serial_port(serial_path);
  if (fd < 0) {
    goto error;
  }

  d = (struct dolphin*)calloc(1, sizeof(struct dolphin));
  d->serial_fd = fd;

  // Check that serial port is working properly
  int res = dolphin_ping_serial(d);
  if (res < 0) {
    goto error;
  }
  return d;
error:
  return NULL;
}

void dolphin_destroy(struct dolphin* d) {
  close(d->serial_fd);
  free(d);
}

int dolphin_toggle_audio(struct dolphin* d) {
  int res;
  uint8_t msg = 1;
  res = write(d->serial_fd, &msg, 1);
  if (res != 1) {
    fprintf(stderr, "Fail to toggle audio\n");
  }
  return res;
}

long diff_us(struct timespec* s, struct timespec* e) {
  long sdiff = e->tv_sec - s->tv_sec;
  long ndiff = e->tv_nsec - s->tv_nsec;
  return sdiff * 1000000 + ndiff / 1000;
}

int dolphin_measure_serial_latency(struct dolphin* d) {
  uint8_t msg = 'l', reply_msg;
  int res;
  struct timespec start, end;

  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  res = write(d->serial_fd, &msg, 1);
  if (res != 1) {
    goto error;
  }

  res = read(d->serial_fd, &reply_msg, 1);
  if (res != 1) {
    goto error;
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);

  fprintf(stderr, "serial port latency: %ld us\n", diff_us(&start, &end));
  return 0;

error:
  fprintf(stderr, "Fail to measure serial latency\n");
  return res;
}

void dolphin_set_level(struct dolphin* d, int8_t level) {
  uint8_t msg = 3;
  int res = write(d->serial_fd, &msg, 1);
  if (res != 1) {
    fprintf(stderr, "Fail to set level\n");
    goto error;
  }
  res = write(d->serial_fd, &level, 1);
  if (res != 1) {
    fprintf(stderr, "Fail to set level\n");
  }
error:
  return;
}
