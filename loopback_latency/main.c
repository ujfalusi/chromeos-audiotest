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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <alsa/asoundlib.h>

#include "args.h"
#include "alsa_helper.h"
#include "common.h"
#include "cras_client.h"

#define TTY_OUTPUT_SIZE 1024

static struct timeval *cras_play_time = NULL;
static struct timeval *cras_cap_time = NULL;
static char *tty_output_dev = NULL;
static FILE *tty_output = NULL;
static const char tty_zeros_block[TTY_OUTPUT_SIZE] = {0};
static pthread_cond_t terminate_test;

static int cras_capture_tone(struct cras_client *client,
                             cras_stream_id_t stream_id,
                             uint8_t *samples, size_t frames,
                             const struct timespec *sample_time,
                             void *arg)
{
    assert(snd_pcm_format_physical_width(g_format) == 16);

    short *data = (short *)samples;
    int cap_frames_index;

    if (!g_sine_started || g_terminate_capture) {
        return frames;
    }

    if ((cap_frames_index = check_for_noise(data, frames, g_channels)) >= 0) {
        fprintf(stderr, "Got noise\n");

        struct timespec shifted_time = *sample_time;
        shifted_time.tv_nsec += 1000000000L / g_rate * cap_frames_index;
        while (shifted_time.tv_nsec > 1000000000L) {
            shifted_time.tv_sec++;
            shifted_time.tv_nsec -= 1000000000L;
        }
        cras_client_calc_capture_latency(&shifted_time, (struct timespec *)arg);
        cras_cap_time = (struct timeval *)malloc(sizeof(*cras_cap_time));
        gettimeofday(cras_cap_time, NULL);

        // Terminate the test since noise got captured.
        pthread_mutex_lock(&g_latency_test_mutex);
        g_terminate_capture = 1;
        pthread_cond_signal(&terminate_test);
        pthread_mutex_unlock(&g_latency_test_mutex);
    }

    return frames;
}

/* Callback for tone playback.  Playback latency will be passed
 * as arg and updated at the first sine tone right after silent.
 */
static int cras_play_tone(struct cras_client *client,
                          cras_stream_id_t stream_id,
                          uint8_t *samples, size_t frames,
                          const struct timespec *sample_time,
                          void *arg)
{
    snd_pcm_channel_area_t *areas;
    int chn;
    size_t sample_bytes;

    sample_bytes = snd_pcm_format_physical_width(g_format) / 8;

    areas = calloc(g_channels, sizeof(snd_pcm_channel_area_t));
    for (chn = 0; chn < g_channels; chn++) {
        areas[chn].addr = samples + chn * sample_bytes;
        areas[chn].first = 0;
        areas[chn].step = g_channels *
                snd_pcm_format_physical_width(g_format);
    }

    if (g_cold)
        goto play_tone;

    /* Write zero first when playback_count < PLAYBACK_SILENT_COUNT
     * or noise got captured. */
    if (g_playback_count < PLAYBACK_SILENT_COUNT) {
        memset(samples, 0, sample_bytes * frames * g_channels);
    } else if (g_playback_count > PLAYBACK_TIMEOUT_COUNT) {
        // Timeout, terminate test.
        pthread_mutex_lock(&g_latency_test_mutex);
        g_terminate_capture = 1;
        pthread_cond_signal(&terminate_test);
        pthread_mutex_unlock(&g_latency_test_mutex);

        /* for loop mode: to avoid underrun */
        memset(samples, 0, sample_bytes * frames * g_channels);
    } else {
play_tone:
        generate_sine(areas, 0, frames, &g_phase);

        if (!g_sine_started) {
            /* Signal that sine tone started playing and update playback time
             * and latency at first played frame. */
            g_sine_started = 1;
            cras_client_calc_playback_latency(sample_time,
                    (struct timespec *)arg);
            cras_play_time =
                    (struct timeval *)malloc(sizeof(*cras_play_time));
            gettimeofday(cras_play_time, NULL);
            if (tty_output) {
                fwrite(tty_zeros_block, sizeof(char),
                       TTY_OUTPUT_SIZE, tty_output);
                fflush(tty_output);
            }
        }
    }

    g_playback_count++;
    return frames;
}

static int stream_error(struct cras_client *client,
                        cras_stream_id_t stream_id,
                        int err,
                        void *arg)
{
    fprintf(stderr, "Stream error %d\n", err);
    return 0;
}

/* Adds stream to cras client.  */
static int cras_add_stream(struct cras_client *client,
                           struct cras_stream_params *params,
                           enum CRAS_STREAM_DIRECTION direction,
                           struct timespec *user_data)
{
    struct cras_audio_format *aud_format;
    cras_playback_cb_t aud_cb;
    cras_error_cb_t error_cb;
    size_t cb_threshold = g_buffer_frames;
    size_t min_cb_level = g_buffer_frames;
    int rc = 0;
    cras_stream_id_t stream_id = 0;

    aud_format = cras_audio_format_create(g_format, g_rate, g_channels);
    if (aud_format == NULL)
        return -ENOMEM;

    /* Create and start stream */
    aud_cb = (direction == CRAS_STREAM_OUTPUT)
            ? cras_play_tone
            : cras_capture_tone;
    error_cb = stream_error;
    params = cras_client_stream_params_create(direction,
            g_buffer_frames,
            cb_threshold,
            min_cb_level,
            0,
            0,
            user_data,
            aud_cb,
            error_cb,
            aud_format);
    if (params == NULL)
        return -ENOMEM;

    if (direction == CRAS_STREAM_INPUT &&
            g_pin_capture_device != PIN_DEVICE_UNSET) {
        rc = cras_client_add_pinned_stream(
            client, g_pin_capture_device, &stream_id, params);
    } else {
        rc = cras_client_add_stream(client, &stream_id, params);
    }
    if (rc < 0) {
        fprintf(stderr, "Add a stream fail.\n");
        return rc;
    }
    cras_audio_format_destroy(aud_format);
    return 0;
}

void cras_test_latency()
{
    int rc;
    struct cras_client *client = NULL;
    struct cras_stream_params *playback_params = NULL;
    struct cras_stream_params *capture_params = NULL;

    struct timespec playback_latency;
    struct timespec capture_latency;

    rc = cras_client_create(&client);
    if (rc < 0) {
        fprintf(stderr, "Create client fail.\n");
        exit(1);
    }
    rc = cras_client_connect(client);
    if (rc < 0) {
        fprintf(stderr, "Connect to server fail.\n");
        cras_client_destroy(client);
        exit(1);
    }

    if (tty_output_dev) {
        tty_output = fopen(tty_output_dev, "w");
        if (!tty_output)
            fprintf(stderr, "Failed to open TTY output device: %s",
                    tty_output_dev);
        else
            fprintf(stdout, "Opened %s for UART signal\n",
                    tty_output_dev);
    }

    pthread_mutex_init(&g_latency_test_mutex, NULL);
    pthread_cond_init(&g_sine_start, NULL);
    pthread_cond_init(&terminate_test, NULL);

    cras_client_run_thread(client);
    // Sleep 500ms to skip input cold start time.
    fprintf(stderr, "Create capture stream and wait for 500ms.\n");
    rc = cras_add_stream(client,
                         capture_params,
                         CRAS_STREAM_INPUT,
                         &capture_latency);
    if (rc < 0) {
        fprintf(stderr, "Fail to add capture stream.\n");
        exit(1);
    }
    struct timespec delay = { .tv_sec = 0, .tv_nsec = 500000000 };
    nanosleep(&delay, NULL);

    fprintf(stderr, "Create playback stream.\n");
    rc = cras_add_stream(client,
                         playback_params,
                         CRAS_STREAM_OUTPUT,
                         &playback_latency);
    if (rc < 0) {
        fprintf(stderr, "Fail to add playback stream.\n");
        exit(1);
    }

again:
    pthread_mutex_lock(&g_latency_test_mutex);
    while (!g_terminate_capture) {
        pthread_cond_wait(&terminate_test, &g_latency_test_mutex);
    }
    pthread_mutex_unlock(&g_latency_test_mutex);

    if (cras_cap_time && cras_play_time) {
        unsigned long playback_latency_us, capture_latency_us;
        unsigned long latency = subtract_timevals(cras_cap_time,
                                                  cras_play_time);
        fprintf(stdout, "Measured Latency: %lu uS.\n", latency);

        playback_latency_us = (playback_latency.tv_sec * 1000000) +
            (playback_latency.tv_nsec / 1000);
        capture_latency_us = (capture_latency.tv_sec * 1000000) +
            (capture_latency.tv_nsec / 1000);

        fprintf(stdout,
                "Reported Latency: %lu uS.\n"
                "Reported Output Latency: %lu uS.\n"
                "Reported Input Latency: %lu uS.\n",
                playback_latency_us + capture_latency_us,
                playback_latency_us,
                capture_latency_us);
        fflush(stdout);
    } else {
        fprintf(stdout, "Audio not detected.\n");
    }

    if (--g_loop > 0) {
        if (cras_play_time)
            free(cras_play_time);
        if (cras_cap_time)
            free(cras_cap_time);
        usleep(50000);
        cras_play_time = cras_cap_time = NULL;
        g_playback_count = 0;

        pthread_mutex_lock(&g_latency_test_mutex);
        g_terminate_capture = 0;
        g_sine_started = 0;
        pthread_mutex_unlock(&g_latency_test_mutex);
        goto again;
    }

    /* Destruct things. */
    cras_client_stop(client);
    cras_client_stream_params_destroy(playback_params);
    cras_client_stream_params_destroy(capture_params);
    if (tty_output)
        fclose(tty_output);
    if (cras_play_time)
        free(cras_play_time);
    if (cras_cap_time)
        free(cras_cap_time);
}

int main (int argc, char *argv[])
{
    int cras_only = 0;
    char *play_dev = NULL;
    char *cap_dev = NULL;

    int arg;
    while ((arg = getopt(argc, argv, "b:i:o:n:r:p:ct:l:CP:s:")) != -1) {
    switch (arg) {
        case 'b':
            g_buffer_frames = atoi(optarg);
            break;
        case 'c':
            cras_only = 1;
            break;
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
        case 't':
            tty_output_dev = optarg;
            break;
        case 'l':
            g_loop = atoi(optarg);
            break;
        case 'C':
            g_cold = 1;
            break;
        case 'P':
            g_pin_capture_device = atoi(optarg);
            fprintf(stderr,
                "Pinning capture device %d\n",
                g_pin_capture_device);
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

    if (cras_only)
        cras_test_latency();
    else {
      if (play_dev == NULL || cap_dev == NULL) {
          fprintf(stderr, "Input/output devices must be set in Alsa mode.\n");
          exit(1);
      }
      alsa_test_latency(play_dev, cap_dev);
    }
    exit(0);
}
