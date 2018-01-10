/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <alsa/asoundlib.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

static void print_supported_capture_channels(const char *dev)
{
    int err;
    unsigned int channels_min = 0, channels_max = 0;
    snd_pcm_hw_params_t *params;
    snd_pcm_t *handle;

    if ((err = snd_pcm_open(&handle, dev, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf(stderr, "Cannot open audio device for capture: %s (%s)\n",
                dev, snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    if ((err = snd_pcm_hw_params_malloc(&params)) < 0) {
        fprintf(stderr, "Cannot allocate hw params structure (%s)\n",
                snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    if ((err = snd_pcm_hw_params_any(handle, params)) < 0) {
        fprintf(stderr, "Cannot initialize hw params structure (%s)\n",
                snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    if ((err = snd_pcm_hw_params_get_channels_min(params, &channels_min)) < 0) {
        fprintf(stderr, "Cannot get channels min (%s)\n",
                snd_strerror(err));
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "channels min=%u\n", channels_min);

    if ((err = snd_pcm_hw_params_get_channels_max(params, &channels_max)) < 0) {
        fprintf(stderr, "Cannot get channels max (%s)\n",
                snd_strerror(err));
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "channels max=%u\n", channels_max);

    for (unsigned int i = channels_min; i <= channels_max; i++) {
        if ((err = snd_pcm_hw_params_test_channels(handle, params, i)) < 0) {
            fprintf(stderr, "Test channels %u failed (%s)\n",
                    i, snd_strerror(err));
        } else {
            printf("%u\n", i);
            fprintf(stderr, "Channels %u is supported\n", i);
        }
    }

    snd_pcm_hw_params_free(params);
    snd_pcm_close(handle);
}

static void print_card_names() {
    int card = -1;
    snd_ctl_t *handle;
    snd_ctl_card_info_t *info;
    snd_ctl_card_info_alloca(&info);

    if (snd_card_next(&card) < 0 || card < 0) {
        fprintf(stderr, "Failed to get devices.\n");
        exit(EXIT_FAILURE);
    }

    while (card >= 0) {
        char name[16];
        sprintf(name, "hw:%d", card);

        if (snd_ctl_open(&handle, name, 0) < 0) {
            fprintf(stderr, "Failed to open device: %s\n", name);
            exit(EXIT_FAILURE);
        }

        if (snd_ctl_card_info(handle, info) < 0) {
            fprintf(stderr, "Failed to get info for device: %s\n", name);
            exit(EXIT_FAILURE);
        }

        printf("%d,%s\n", card, snd_ctl_card_info_get_name(info));

        snd_ctl_close(handle);

        if (snd_card_next(&card) < 0) {
            fprintf(stderr, "Failed to get next card\n");
            exit(EXIT_FAILURE);
        }
    }
}

/* This program has helper functions that are used by autotests. The returned
   values are printed in stdout. Debug and error logs are printed in stderr.
   If the program fails to get the requested value, it returns 1. */
int main(int argc, char *argv[])
{
    int c, get_capture_channels = 0, list_card_names = 0;
    const char *short_opt = "hd:cl";
    char *dev = "hw:0,0";

    struct option long_opt[] =
    {
       {"help",                  no_argument,       NULL, 'h'},
       {"device",                required_argument, NULL, 'd'},
       {"get_capture_channels",  no_argument,       NULL, 'c'},
       {"list_card_names",       no_argument,       NULL, 'l'},
       {NULL,                    0,                 NULL, 0  }
    };

    while(1) {
       c = getopt_long(argc, argv, short_opt, long_opt, NULL);
       if (c == -1)
           break;
       switch(c) {
       case 'd':
           dev = optarg;
           fprintf(stderr, "Assign device to %s\n", dev);
           break;

       case 'c':
           get_capture_channels = 1;
           break;

       case 'l':
           list_card_names = 1;
           break;

       case 'h':
           printf("Usage: %s [OPTIONS]\n", argv[0]);
           printf("  -d, --device <Device>       Device, default to hw:0,0\n");
           printf("  -h, --help                  Print this help and exit\n");
           printf("  -c, --get_capture_channels  Get supported channels of the "
                                                "capture device\n");
           printf("  -l, --list_card_names       List all cards including "
                                                "names\n");
           return(EXIT_SUCCESS);

       default:
           fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
           exit(EXIT_FAILURE);
       }
    }

    if (get_capture_channels) {
        print_supported_capture_channels(dev);
        exit(EXIT_SUCCESS);
    }

    if (list_card_names) {
        print_card_names();
        exit(EXIT_SUCCESS);
    }

    fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
    exit(EXIT_FAILURE);
}
