// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "../latency_libraries/alsa_helper.h"
#include "../latency_libraries/args.h"
#include "../latency_libraries/common.h"
#include "../latency_libraries/cras_helper.h"
#include "../latency_libraries/dolphin.h"

int main(int argc, char* argv[]) {
  char c;
  int opt;
  int8_t level;
  const char* short_opt = "hsta:l:cf:b:p:";
  enum BACKEND backend = ALSA;
  char* play_dev = NULL;

  static struct option long_opt[] = {
      {"help", no_argument, NULL, 'h'},
      {"serial_latency", no_argument, NULL, 's'},
      {"toggle_audio_playback", no_argument, NULL, 't'},
      {"alsa_output_latency", required_argument, NULL, 'a'},
      {"level", required_argument, NULL, 'l'},
      {"cras_output_latency", no_argument, NULL, 'c'},
      {"format", required_argument, NULL, 'f'},
      {"buffer_size", required_argument, NULL, 'b'},
      {"period_size", required_argument, NULL, 'p'},
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
        play_dev = optarg;
        backend = ALSA;
        break;
#ifdef WITH_CRAS
      case 'c':
        backend = CRAS;
        break;
#endif
      case 'f':
        set_format(optarg);
        break;
      case 'b':
        g_buffer_frames = atoi(optarg);
        break;
      case 'p':
        g_period_size = atoi(optarg);
        break;
      default:
        fprintf(stderr, "no such command.\n");
        exit(-1);
    }
  }
  switch (backend) {
    case ALSA:
      if (play_dev == NULL) {
        fprintf(stderr, "Output device must be set in Alsa mode.\n");
        exit(1);
      }
      teensy_alsa_test_latency(play_dev, d);
      break;
#ifdef WITH_CRAS
    case CRAS:
      teensy_cras_test_latency(d);
      break;
#endif
  }
  get_teensy_capture_result(d);
  dolphin_destroy(d);
  return 0;
}
