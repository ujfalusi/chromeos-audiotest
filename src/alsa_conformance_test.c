/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <alsa/asoundlib.h>
#include <getopt.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "include/alsa_conformance_args.h"
#include "include/alsa_conformance_debug.h"
#include "include/alsa_conformance_helper.h"
#include "include/alsa_conformance_thread.h"

int DEBUG_MODE = false;
int SINGLE_THREAD;

void show_usage(const char *name)
{
    printf("Usage: %s [OPTIONS]\n", name);
    printf("\t-h, --help: Print this help and exit.\n");
    printf("\t-P, --playback_dev <device>: "
           "PCM device for playback. (default: NULL)\n");
    printf("\t-C, --capture_dev <device>: "
           "PCM device for capture. (default: NULL)\n");
    printf("\t-c, --channels <channels>: Set channels. (default: 2)\n");
    printf("\t-f, --format <format>: Set format. (default: S16_LE)\n");
    printf("\t-r, --rate <rate>: Set rate. (default: 48000)\n");
    printf("\t-p, --period <period>: Set period. (default: 240)\n");
    printf("\t-d, --durations <duration>: "
           "Set durations(second). (default: 1.0)\n");
    printf("\t-B, --block_size <block_size>: "
           "Set block size in frames of each write. (default: 240)\n");
    printf("\t-D, --debug: "
           "Enable debug mode. (Not support multi-streams in this version)\n");
}

void set_dev_thread_args(struct dev_thread *thread,
                         struct alsa_conformance_args *args)
{
    dev_thread_set_channels(thread, args_get_channels(args));
    dev_thread_set_format(thread, args_get_format(args));
    dev_thread_set_rate(thread, args_get_rate(args));
    dev_thread_set_period_size(thread, args_get_period_size(args));
    dev_thread_set_block_size(thread, args_get_block_size(args));
    dev_thread_set_duration(thread, args_get_duration(args));
}

struct dev_thread* create_playback_thread(struct alsa_conformance_args *args)
{
    struct dev_thread *thread;
    thread = dev_thread_create();
    set_dev_thread_args(thread, args);
    dev_thread_set_stream(thread, SND_PCM_STREAM_PLAYBACK);
    dev_thread_set_dev_name(thread, args_get_playback_dev_name(args));
    return thread;
}

struct dev_thread* create_capture_thread(struct alsa_conformance_args *args)
{
    struct dev_thread *thread;
    thread = dev_thread_create();
    set_dev_thread_args(thread, args);
    dev_thread_set_stream(thread, SND_PCM_STREAM_CAPTURE);
    dev_thread_set_dev_name(thread, args_get_capture_dev_name(args));
    return thread;
}

void* alsa_conformance_run_thread(void *arg)
{
    struct dev_thread *thread = arg;
    dev_thread_run(thread);
    return 0;
}

void alsa_conformance_run(struct alsa_conformance_args *args)
{
    /* Only support one playback and one capture device now. */
    struct dev_thread *thread_list[2];
    pthread_t thread_id[2];
    size_t thread_count;
    int i;

    thread_count = 0;

    if (args_get_playback_dev_name(args)) {
        thread_list[thread_count++] = create_playback_thread(args);
    }

    if (args_get_capture_dev_name(args)) {
        thread_list[thread_count++] = create_capture_thread(args);
    }

    if (!thread_count) {
        puts("No device selected.");
        exit(EXIT_FAILURE);
    }

    if (thread_count > 1) {
       /* TODO(yuhsuan): Maybe we will support debug mode in multi-streams. */
       if (DEBUG_MODE) {
            puts("[Notice] Disable debug mode because of multi-threads.");
            DEBUG_MODE = false;
        }
        SINGLE_THREAD = false;
    } else {
        SINGLE_THREAD = true;
    }

    for (i = 0; i < thread_count; i++) {
        dev_thread_device_open(thread_list[i]);
        if (SINGLE_THREAD)
            dev_thread_print_device_information(thread_list[i]);
        dev_thread_set_params(thread_list[i]);
    }

    for (i = 0; i < thread_count; i++)
        pthread_create(&thread_id[i],
                       NULL,
                       alsa_conformance_run_thread,
                       thread_list[i]);

    for (i = 0; i < thread_count; i++)
        pthread_join(thread_id[i],NULL);

    for (i = 0; i < thread_count; i++) {
        if (!SINGLE_THREAD)
            puts("=============================================");
        dev_thread_print_result(thread_list[i]);
        dev_thread_destroy(thread_list[i]);
        if (!SINGLE_THREAD)
            puts("=============================================");
    }
}

void parse_arguments(struct alsa_conformance_args *test_args,
                     int argc,
                     char *argv[])
{
    int c;
    const char *short_opt = "hP:C:c:f:r:p:B:d:D";
    static struct option long_opt[] =
    {
        {"help",         no_argument,       NULL, 'h'},
        {"playback_dev", required_argument, NULL, 'P'},
        {"capture_dev",  required_argument, NULL, 'C'},
        {"channel",      required_argument, NULL, 'c'},
        {"format",       required_argument, NULL, 'f'},
        {"rate",         required_argument, NULL, 'r'},
        {"period",       required_argument, NULL, 'p'},
        {"block_size",   required_argument, NULL, 'B'},
        {"durations",    required_argument, NULL, 'd'},
        {"debug",        no_argument,       NULL, 'D'},
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

        case 'C':
            args_set_capture_dev_name(test_args, optarg);
            break;

        case 'h':
            show_usage(argv[0]);
            exit(0);
            break;

        case 'c':
            args_set_channels(test_args, (unsigned int) atoi(optarg));
            break;

        case 'f':
            args_set_format(test_args, optarg);
            break;

        case 'r':
            args_set_rate(test_args, (unsigned int) atoi(optarg));
            break;

        case 'p':
            args_set_period_size(test_args, (unsigned int) atoi(optarg));
            break;

        case 'B':
            args_set_block_size(test_args, (unsigned int) atoi(optarg));
            break;

        case 'd':
            args_set_duration(test_args, (double) atof(optarg));
            break;

        case 'D':
            DEBUG_MODE = true;
            puts("Enable debug mode!");
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
