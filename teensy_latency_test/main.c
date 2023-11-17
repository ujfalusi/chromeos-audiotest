// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "include/alsa_helper.h"
#include "include/cras_helper.h"

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

const char serial_path[] = "/dev/ttyACM0";

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

int dolphin_output_latency_alsa(struct dolphin* d, char* dev) {
  int res;
  snd_pcm_t* handle = opendev(dev);

  play(handle, d);
  printf("end play\n");
  char buf[100];
  res = read(d->serial_fd, buf, 100);
  if (res <= 0) {
    fprintf(stderr, "Fail to read latency\n");
  } else {
    printf("latency: %s\n", buf);
  }
  printf("turn off audio\n");

  return res;
}

int dolphin_output_latency_cras(struct dolphin* d, char* dev) {
  int res;

  cras_test_latency(d);
  printf("end play\n");
  char buf[100];
  res = read(d->serial_fd, buf, 100);
  if (res <= 0) {
    fprintf(stderr, "Fail to read latency\n");
  } else {
    printf("latency: %s\n", buf);
  }
  printf("turn off audio\n");

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

int main(int argc, char* argv[]) {
  char c;
  int opt;
  int8_t level;
  const char* short_opt = "hsta:l:cf:";
  static struct option long_opt[] = {
      {"help", no_argument, NULL, 'h'},
      {"serial_latency", no_argument, NULL, 's'},
      {"toggle_audio_playback", no_argument, NULL, 't'},
      {"alsa_output_latency", no_argument, NULL, 'a'},
      {"level", no_argument, NULL, 'l'},
      {"cras_output_latency", no_argument, NULL, 'c'},
      {"format", no_argument, NULL, 'f'},
  };

  struct dolphin* d = dolphin_create(serial_path);
  if (!d) {
    fprintf(stderr, "Failed to create dolphin.\n");
    exit(1);
  }

  while (1) {
    opt = getopt_long(argc, argv, short_opt, long_opt, NULL);
    if (opt == -1) {
      break;
    }
    c = opt;
    switch (c) {
      case 'l':
        level = atoi(optarg);
        printf("level %hhd\n", level);
        dolphin_set_level(d, level);
        break;
      case 't':
        dolphin_toggle_audio(d);
        break;
      case 's':
        dolphin_measure_serial_latency(d);
        break;
      case 'a':
        dolphin_output_latency_alsa(d, optarg);
        break;
      case 'c':
        dolphin_output_latency_cras(d, optarg);
        break;
      case 'f':
        set_format(optarg);
        break;
      default:
        fprintf(stderr, "no such command.\n");
        exit(-1);
    }
  }

  dolphin_destroy(d);
  return 0;
}
