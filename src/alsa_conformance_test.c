/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <alsa/asoundlib.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "include/alsa_conformance_args.h"
#include "include/alsa_conformance_helper.h"
#include "include/alsa_conformance_thread.h"

void show_usage(const char *name)
{
    printf("Usage: %s [OPTIONS]\n", name);
    printf("\t-h, --help: Print this help and exit.\n");
    printf("\t-P, --playback_dev <device>: "
           "PCM device for playback. (default: hw:0,0)\n");
}

void set_dev_thread_args(struct dev_thread *thread,
                         struct alsa_conformance_args *args)
{
    dev_thread_set_stream(thread, SND_PCM_STREAM_PLAYBACK);
    dev_thread_set_dev_name(thread, args_get_playback_dev_name(args));
}

void alsa_conformance_run(struct alsa_conformance_args *args)
{
    /* Only support one playback device now. */
    struct dev_thread *thread;

    thread = dev_thread_create();
    set_dev_thread_args(thread, args);
    dev_thread_device_open(thread);
    dev_thread_print_device_information(thread);
    dev_thread_destroy(thread);
}

void parse_arguments(struct alsa_conformance_args *test_args,
                     int argc,
                     char *argv[])
{
    int c;
    const char *short_opt = "hP:";
    static struct option long_opt[] =
    {
        {"help",         no_argument,       NULL, 'h'},
        {"playback_dev", required_argument, NULL, 'P'},
        {0, 0, 0, 0}
    };
    while (1) {
        c = getopt_long(argc, argv, short_opt, long_opt, NULL);
        if (c == -1)
            break;
        switch (c) {
        case 'P':
            args_set_playback_dev_name(test_args, optarg);
            break;

        case 'h':
            show_usage(argv[0]);
            exit(0);
            break;

        case ':':
        case '?':
            fprintf(stderr, "Try `%s --help' for more information.\n",
                argv[0]);
            exit(-1);

        default:
            fprintf(stderr, "%s: invalid option -- %c\n", argv[0], c);
            fprintf(stderr, "Try `%s --help' for more information.\n",
                argv[0]);
            exit(-1);
        }
    }
}

int main(int argc, char *argv[])
{
    struct alsa_conformance_args *test_args;
    test_args = args_create();
    parse_arguments(test_args, argc, argv);
    alsa_conformance_run(test_args);
    args_destroy(test_args);
}
