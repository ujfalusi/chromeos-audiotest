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

#include "alsa_conformance_args.h"
#include "alsa_conformance_debug.h"
#include "alsa_conformance_helper.h"
#include "alsa_conformance_thread.h"

#define MAX_DEVICES 10

int DEBUG_MODE = false;
int SINGLE_THREAD;
int STRICT_MODE = false;

void show_usage(const char *name)
{
	printf("Alsa Conformance Test\n\n"
	       "This tool is used to verify the correctness and performance of "
	       "audio drivers.\nIt can be also used to verify the quality of "
	       "audio bringup and prevent regression.\n\nTo start with, first get "
	       "the playback device via `aplay -l` or get the capture device with "
	       "`arecord -l`.\nTo test the playback functionality with default "
	       "arguments, please run `alsa_conformance_test -P hw:<sound_card>,"
	       "<device>`.\nThis tool also supports testing playback and capture "
	       "simultaneously.\n\nFor convenience, we provide a script called "
	       "`alsa_conformance_test.py`. It runs this test with different "
	       "parameter sets.\n\nFor more detailed documentation, please read:"
	       "\n\n\thttps://chromium.googlesource.com/chromiumos/platform/audiotest/+/HEAD/alsa_conformance_test.md\n\n");
	printf("Usage: %s [OPTIONS]\n", name);
	printf("\t-h, --help: Print this help and exit.\n");
	printf("\t-P, --playback_dev <device>: "
	       "PCM device for playback. (default: NULL)\n");
	printf("\t-C, --capture_dev <device>: "
	       "PCM device for capture. (default: NULL)\n");
	printf("\t-c, --channels <channels>: Set channels. (default: 2)\n");
	printf("\t-f, --format <format>: Set format. (default: S16_LE)\n");
	printf("\t-r, --rate <rate>: Set rate. (default: 48000)\n");
	printf("\t-p, --period <period>: Set period size. If not set, the default"
	       " value set in the driver will be used. \n");
	printf("\t-d, --durations <duration>: "
	       "Set durations(second). (default: 1.0)\n");
	printf("\t-B, --block_size <block_size>: "
	       "Set block size in frames of each write. (default: 240)\n");
	printf("\t--debug: "
	       "Enable debug mode. (Not support multi-streams in this version)\n");
	printf("\t--strict: "
	       "Enable strict mode. It will set params to the fixed value.\n");
	printf("\t--dev_info_only: "
	       "Show device information only without setting params and running "
	       "I/O.\n");
	printf("\t--iterations: "
	       "Number of times to run the tests specified. (default: 1)\n");
	printf("\t--merge_threshold: "
	       "Set merge_threshold_t. (default: 0.0001)\n"
	       "\t\tPoints with TIME_DIFF less than merge_threshold_t and SAMPLES_DIFF less\n"
	       "\t\tthan merge_threshold_sz will be merged.\n");
	printf("\t--device_file:\n"
	       "\t\tDevice file path. It will load devices from the file. "
	       "File format:\n"
	       "\t\t[name] [type] [channels] [format] [rate] [period] [block_size]"
	       " [durations] # comment\n"
	       "\t\t[type] could be either `PLAYBACK` or `CAPTURE`. # comment\n"
	       "\t\teg: hw:0,0 PLAYBACK 2 S16_LE 48000 240 240 10 # Example\n");
	printf("\t--merge_threshold_sz: "
	       "Set frame merge threadhold size, set to period size if not set\n"
	       "\t\tPoints with TIME_DIFF less than merge_threshold_t and SAMPLES_DIFF less\n"
	       "\t\tthan merge_threshold_sz will be merged.\n");
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
	dev_thread_set_iterations(thread, args_get_iterations(args));
	dev_thread_set_merge_threshold_t(thread,
					 args_get_merge_threshold(args));
	dev_thread_set_merge_threshold_size(thread,
					 args_get_merge_threshold_sz(args));
}

struct dev_thread *create_playback_thread(struct alsa_conformance_args *args)
{
	struct dev_thread *thread;
	thread = dev_thread_create();
	set_dev_thread_args(thread, args);
	dev_thread_set_stream(thread, SND_PCM_STREAM_PLAYBACK);
	dev_thread_set_dev_name(thread, args_get_playback_dev_name(args));
	return thread;
}

struct dev_thread *create_capture_thread(struct alsa_conformance_args *args)
{
	struct dev_thread *thread;
	thread = dev_thread_create();
	set_dev_thread_args(thread, args);
	dev_thread_set_stream(thread, SND_PCM_STREAM_CAPTURE);
	dev_thread_set_dev_name(thread, args_get_capture_dev_name(args));
	return thread;
}

size_t parse_device_file(struct alsa_conformance_args *args,
			 struct dev_thread **thread_list)
{
	FILE *fp;
	const char *file_name;
	size_t thread_count;
	struct dev_thread *thread;
	char name[20];
	char type[20];
	unsigned int channels;
	char format[20];
	unsigned int rate;
	snd_pcm_uframes_t period_size;
	unsigned int block_size;
	double duration;
	int rc;
	char buf[1000];
	char *p;

	thread_count = 0;

	file_name = args_get_device_file(args);
	fp = fopen(file_name, "r");
	if (fp == NULL) {
		fprintf(stderr, "Open file %s fail: %s\n", file_name,
			strerror(errno));
		exit(EXIT_FAILURE);
	}

	/*
     * Format of device file:
     * [name] [type] [channels] [format] [rate] [period] [block] [duration]
     * # comment
     */
	while (1) {
		if (fgets(buf, 1000, fp) == NULL)
			break;
		p = strchr(buf, '#');
		if (p)
			*p = 0;

		rc = sscanf(buf, "%15s %15s %u %15s %u %lu %u %lf", name, type,
			    &channels, format, &rate, &period_size, &block_size,
			    &duration);
		if (rc != 8)
			continue;

		thread = dev_thread_create();
		dev_thread_set_dev_name(thread, name);
		if (strcmp(type, "CAPTURE") == 0) {
			dev_thread_set_stream(thread, SND_PCM_STREAM_CAPTURE);
		} else if (strcmp(type, "PLAYBACK") == 0) {
			dev_thread_set_stream(thread, SND_PCM_STREAM_PLAYBACK);
		} else {
			fprintf(stderr, "Unknown type %s\n", type);
			exit(EXIT_FAILURE);
		}
		dev_thread_set_channels(thread, channels);
		dev_thread_set_format_from_str(thread, format);
		dev_thread_set_rate(thread, rate);
		dev_thread_set_period_size(thread, period_size);
		dev_thread_set_block_size(thread, block_size);
		dev_thread_set_duration(thread, duration);
		dev_thread_set_iterations(thread, args_get_iterations(args));

		thread_list[thread_count++] = thread;
	}
	return thread_count;
}

void alsa_conformance_run(struct alsa_conformance_args *args)
{
	struct dev_thread *thread_list[MAX_DEVICES];
	pthread_t thread_id[MAX_DEVICES];
	size_t thread_count;
	int i;

	/*
	 * If we have device file, load devices from the file. Otherwise, load
	 * devices from arguments.
	 */
	if (args_get_device_file(args)) {
		thread_count = parse_device_file(args, thread_list);
	} else {
		thread_count = 0;
		if (args_get_playback_dev_name(args)) {
			thread_list[thread_count++] =
				create_playback_thread(args);
		}
		if (args_get_capture_dev_name(args)) {
			thread_list[thread_count++] =
				create_capture_thread(args);
		}
	}

	if (!thread_count) {
		puts("No device selected.");
		return;
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

	if (args_get_dev_info_only(args)) {
		for (i = 0; i < thread_count; i++) {
			puts("------DEVICE INFORMATION------");
			dev_thread_open_device(thread_list[i]);
			dev_thread_print_device_information(thread_list[i]);
			dev_thread_close_device(thread_list[i]);
			dev_thread_destroy(thread_list[i]);
			puts("------------------------------");
		}
		return;
	}

	for (i = 0; i < thread_count; i++)
		pthread_create(&thread_id[i], NULL, dev_thread_run_iterations,
			       thread_list[i]);

	for (i = 0; i < thread_count; i++)
		pthread_join(thread_id[i], NULL);

	for (i = 0; i < thread_count; i++) {
		if (!SINGLE_THREAD)
			puts("=============================================");
		dev_thread_print_result(thread_list[i]);
		dev_thread_destroy(thread_list[i]);
		if (!SINGLE_THREAD)
			puts("=============================================");
	}
}

void parse_arguments(struct alsa_conformance_args *test_args, int argc,
		     char *argv[])
{
	enum OPTION {
		OPT_DEBUG = 300,
		OPT_DEVICE_FILE,
		OPT_STRICT,
		OPT_DEV_INFO_ONLY,
		OPT_ITERATIONS,
		OPT_MERGE_THRESHOLD,
		OPT_MERGE_THRESHOLD_SZ
	};
	int c;
	const char *short_opt = "hP:C:c:f:r:p:B:d:D";
	static struct option long_opt[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "playback_dev", required_argument, NULL, 'P' },
		{ "capture_dev", required_argument, NULL, 'C' },
		{ "channel", required_argument, NULL, 'c' },
		{ "format", required_argument, NULL, 'f' },
		{ "rate", required_argument, NULL, 'r' },
		{ "period", required_argument, NULL, 'p' },
		{ "block_size", required_argument, NULL, 'B' },
		{ "durations", required_argument, NULL, 'd' },
		{ "debug", no_argument, NULL, OPT_DEBUG },
		{ "device_file", required_argument, NULL, OPT_DEVICE_FILE },
		{ "strict", no_argument, NULL, OPT_STRICT },
		{ "dev_info_only", no_argument, NULL, OPT_DEV_INFO_ONLY },
		{ "iterations", required_argument, NULL, OPT_ITERATIONS },
		{ "merge_threshold", required_argument, NULL,
		  OPT_MERGE_THRESHOLD },
		{ "merge_threshold_sz", required_argument, NULL,
		  OPT_MERGE_THRESHOLD_SZ },
		{ 0, 0, 0, 0 }
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
			args_set_channels(test_args,
					  (unsigned int)atoi(optarg));
			break;

		case 'f':
			args_set_format(test_args, optarg);
			break;

		case 'r':
			args_set_rate(test_args, (unsigned int)atoi(optarg));
			break;

		case 'p':
			args_set_period_size(test_args,
					     (unsigned int)atoi(optarg));
			break;

		case 'B':
			args_set_block_size(test_args,
					    (unsigned int)atoi(optarg));
			break;

		case 'd':
			args_set_duration(test_args, (double)atof(optarg));
			break;

		case OPT_DEBUG:
			DEBUG_MODE = true;
			puts("Enable debug mode!");
			break;

		case OPT_DEVICE_FILE:
			args_set_device_file(test_args, optarg);
			break;

		case OPT_STRICT:
			STRICT_MODE = true;
			puts("Enable strict mode!");
			break;

		case OPT_DEV_INFO_ONLY:
			args_set_dev_info_only(test_args, true);
			break;

		case OPT_ITERATIONS:
			args_set_iterations(test_args, atoi(optarg));
			break;

		case OPT_MERGE_THRESHOLD:
			args_set_merge_threshold(test_args,
						 (double)atof(optarg));
			break;

		case OPT_MERGE_THRESHOLD_SZ:
			args_set_merge_threshold_sz(test_args,
						 (int)atof(optarg));
			break;
		case ':':
		case '?':
			fprintf(stderr,
				"Try `%s --help' for more information.\n",
				argv[0]);
			exit(-1);

		default:
			fprintf(stderr, "%s: invalid option -- %c\n", argv[0],
				c);
			fprintf(stderr,
				"Try `%s --help' for more information.\n",
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
	return 0;
}
