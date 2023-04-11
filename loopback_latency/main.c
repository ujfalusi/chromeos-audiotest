/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "alsa_helper.h"
#include "args.h"
#include "common.h"

#ifdef WITH_CRAS
#include "cras_helper.h"
#endif

enum BACKEND {
  ALSA = 0,
#ifdef WITH_CRAS
  CRAS
#endif
};

int main(int argc, char* argv[]) {
  enum BACKEND backend = ALSA;
  char* play_dev = NULL;
  char* cap_dev = NULL;

  int arg;
  while ((arg = getopt(argc, argv, "b:i:o:n:r:p:ct:l:CP:s:")) != -1) {
    switch (arg) {
      case 'b':
        g_buffer_frames = atoi(optarg);
        break;
#ifdef WITH_CRAS
      case 'c':
        backend = CRAS;
        break;
#endif
      case 'i':
        cap_dev = optarg;
        fprintf(stderr, "Assign cap_dev %s\n", cap_dev);
        break;
      case 'n':
        g_noise_threshold = atoi(optarg);
        break;
      case 'r':
        g_rate = atoi(optarg);
        break;
      case 'o':
        play_dev = optarg;
        fprintf(stderr, "Assign play_dev %s\n", play_dev);
        break;
      case 'p':
        g_period_size = atoi(optarg);
        break;
#ifdef WITH_CRAS
      case 't':
        tty_output_dev = optarg;
        break;
#endif
      case 'l':
        g_loop = atoi(optarg);
        break;
      case 'C':
        g_cold = 1;
        break;
      case 'P':
        g_pin_capture_device = atoi(optarg);
        fprintf(stderr, "Pinning capture device %d\n", g_pin_capture_device);
        break;
      case 's':
        g_start_threshold = atoi(optarg);
        break;
      default:
        return 1;
    }
  }

  if (g_loop && g_cold) {
    fprintf(stderr, "Cold and loop are exclusive.\n");
    exit(1);
  }

  switch (backend) {
    case ALSA:
      if (play_dev == NULL || cap_dev == NULL) {
        fprintf(stderr, "Input/output devices must be set in Alsa mode.\n");
        exit(1);
      }
      alsa_test_latency(play_dev, cap_dev);
      break;
#ifdef WITH_CRAS
    case CRAS:
      cras_test_latency();
      break;
#endif
  }
  exit(0);
}
