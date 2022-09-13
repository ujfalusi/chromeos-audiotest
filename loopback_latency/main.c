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
#include "common.h"
#include "cras_client.h"

#define TTY_OUTPUT_SIZE 1024

static struct timeval *cras_play_time = NULL;
static struct timeval *cras_cap_time = NULL;
static char *tty_output_dev = NULL;
static FILE *tty_output = NULL;
static const char tty_zeros_block[TTY_OUTPUT_SIZE] = {0};
static pthread_cond_t terminate_test;

static snd_pcm_sframes_t playback_delay_frames;
static struct timeval sine_start_tv;

static void config_pcm_hw_params(snd_pcm_t *handle,
                                 unsigned int rate,
                                 unsigned int channels,
                                 snd_pcm_format_t format,
                                 snd_pcm_uframes_t *buffer_size,
                                 snd_pcm_uframes_t *period_size)
{
    int err;
    snd_pcm_hw_params_t *hw_params;

    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
        fprintf(stderr, "cannot allocate hardware parameter structure (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_any(handle, hw_params)) < 0) {
        fprintf(stderr, "cannot initialize hardware parameter structure (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_access(handle, hw_params,
            SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf(stderr, "cannot set access type (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_format(handle, hw_params,
            format)) < 0) {
        fprintf(stderr, "cannot set sample format (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_rate_near(
            handle, hw_params, &rate, 0)) < 0) {
        fprintf(stderr, "cannot set sample rate (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_channels(handle, hw_params, 2)) < 0) {
        fprintf(stderr, "cannot set channel count (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_buffer_size_near(
            handle, hw_params, buffer_size)) < 0) {
        fprintf(stderr, "cannot set buffer size (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_period_size_near(
            handle, hw_params, period_size, 0)) < 0) {
        fprintf(stderr, "cannot set period size (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params(handle, hw_params)) < 0) {
        fprintf(stderr, "cannot set hardware parameters (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    snd_pcm_hw_params_free(hw_params);

    if ((err = snd_pcm_prepare(handle)) < 0) {
        fprintf(stderr, "cannot prepare audio interface for use (%s)\n",
                snd_strerror(err));
        exit(1);
    }
}

static void config_pcm_sw_params(snd_pcm_t *handle,
                                 snd_pcm_uframes_t start_threshold)
{
    int err;
    snd_pcm_sw_params_t *sw_params;

    if ((err = snd_pcm_sw_params_malloc(&sw_params)) < 0) {
        fprintf(stderr, "cannot allocate software parameter structure (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_sw_params_current(handle, sw_params)) < 0) {
        fprintf(stderr, "cannot get current sw parameter structure (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    if (start_threshold > 0) {
        if ((err = snd_pcm_sw_params_set_start_threshold(handle,
                sw_params, start_threshold)) < 0) {
            fprintf(stderr, "cannot set start threshold (%s)\n",
                    snd_strerror(err));
            exit(1);
        }
    }

    if (snd_pcm_sw_params(handle, sw_params) < 0) {
        fprintf(stderr, "cannot set software parameters (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    snd_pcm_sw_params_free(sw_params);
}

static int capture_some(snd_pcm_t *pcm, short *buf, unsigned len,
                        snd_pcm_sframes_t *cap_delay_frames)
{
    snd_pcm_sframes_t frames = snd_pcm_avail(pcm);
    int err;

    if (frames > 0) {
        frames = frames > len ? len : frames;

        snd_pcm_delay(pcm, cap_delay_frames);
        if ((err = snd_pcm_readi(pcm, buf, frames)) != frames) {
            fprintf(stderr, "read from audio interface failed (%s)\n",
                    snd_strerror(err));
            exit(1);
        }
    }

    return (int)frames;
}

static int cras_capture_tone(struct cras_client *client,
                             cras_stream_id_t stream_id,
                             uint8_t *samples, size_t frames,
                             const struct timespec *sample_time,
                             void *arg)
{
    assert(snd_pcm_format_physical_width(format) == 16);

    short *data = (short *)samples;
    int cap_frames_index;

    if (!sine_started || terminate_capture) {
        return frames;
    }

    if ((cap_frames_index = check_for_noise(data, frames, channels)) >= 0) {
        fprintf(stderr, "Got noise\n");

        struct timespec shifted_time = *sample_time;
        shifted_time.tv_nsec += 1000000000L / rate * cap_frames_index;
        while (shifted_time.tv_nsec > 1000000000L) {
            shifted_time.tv_sec++;
            shifted_time.tv_nsec -= 1000000000L;
        }
        cras_client_calc_capture_latency(&shifted_time, (struct timespec *)arg);
        cras_cap_time = (struct timeval *)malloc(sizeof(*cras_cap_time));
        gettimeofday(cras_cap_time, NULL);

        // Terminate the test since noise got captured.
        pthread_mutex_lock(&latency_test_mutex);
        terminate_capture = 1;
        pthread_cond_signal(&terminate_test);
        pthread_mutex_unlock(&latency_test_mutex);
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

    sample_bytes = snd_pcm_format_physical_width(format) / 8;

    areas = calloc(channels, sizeof(snd_pcm_channel_area_t));
    for (chn = 0; chn < channels; chn++) {
        areas[chn].addr = samples + chn * sample_bytes;
        areas[chn].first = 0;
        areas[chn].step = channels *
                snd_pcm_format_physical_width(format);
    }

    if (cold)
        goto play_tone;

    /* Write zero first when playback_count < PLAYBACK_SILENT_COUNT
     * or noise got captured. */
    if (playback_count < PLAYBACK_SILENT_COUNT) {
        memset(samples, 0, sample_bytes * frames * channels);
    } else if (playback_count > PLAYBACK_TIMEOUT_COUNT) {
        // Timeout, terminate test.
        pthread_mutex_lock(&latency_test_mutex);
        terminate_capture = 1;
        pthread_cond_signal(&terminate_test);
        pthread_mutex_unlock(&latency_test_mutex);

        /* for loop mode: to avoid underrun */
        memset(samples, 0, sample_bytes * frames * channels);
    } else {
play_tone:
        generate_sine(areas, 0, frames, &phase);

        if (!sine_started) {
            /* Signal that sine tone started playing and update playback time
             * and latency at first played frame. */
            sine_started = 1;
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

    playback_count++;
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
    size_t cb_threshold = buffer_frames;
    size_t min_cb_level = buffer_frames;
    int rc = 0;
    cras_stream_id_t stream_id = 0;

    aud_format = cras_audio_format_create(format, rate, channels);
    if (aud_format == NULL)
        return -ENOMEM;

    /* Create and start stream */
    aud_cb = (direction == CRAS_STREAM_OUTPUT)
            ? cras_play_tone
            : cras_capture_tone;
    error_cb = stream_error;
    params = cras_client_stream_params_create(direction,
            buffer_frames,
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
            pin_capture_device != PIN_DEVICE_UNSET) {
        rc = cras_client_add_pinned_stream(
            client, pin_capture_device, &stream_id, params);
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

static void *alsa_play(void *arg) {
    snd_pcm_t *handle = (snd_pcm_t *)arg;
    short *play_buf;
    snd_pcm_channel_area_t *areas;
    unsigned int chn, num_buffers;
    int err;

    play_buf = calloc(buffer_frames * channels, sizeof(play_buf[0]));
    areas = calloc(channels, sizeof(snd_pcm_channel_area_t));

    for (chn = 0; chn < channels; chn++) {
        areas[chn].addr = play_buf;
        areas[chn].first = chn * snd_pcm_format_physical_width(format);
        areas[chn].step = channels * snd_pcm_format_physical_width(format);
    }

    for (num_buffers = 0; num_buffers < PLAYBACK_SILENT_COUNT; num_buffers++) {
        if ((err = snd_pcm_writei(handle, play_buf, period_size))
                != period_size) {
            fprintf(stderr, "write %dth silent block to audio interface \
                    failed (%s)\n", num_buffers, snd_strerror(err));
            exit(1);
        }
    }

    generate_sine(areas, 0, period_size, &phase);
    snd_pcm_delay(handle, &playback_delay_frames);
    gettimeofday(&sine_start_tv, NULL);

    num_buffers = 0;
    int avail_frames;

    /* Play a sine wave and look for it on capture thread.
     * This will fail for latency > 500mS. */
    while (!terminate_playback && num_buffers < PLAYBACK_COUNT) {
        avail_frames = snd_pcm_avail(handle);
        if (avail_frames >= period_size) {
            pthread_mutex_lock(&latency_test_mutex);
            if (!sine_started) {
                sine_started = 1;
                pthread_cond_signal(&sine_start);
            }
            pthread_mutex_unlock(&latency_test_mutex);
            if ((err = snd_pcm_writei(handle, play_buf, period_size))
                    != period_size) {
                fprintf(stderr, "write to audio interface failed (%s)\n",
                        snd_strerror(err));
            }
            num_buffers++;
        }
    }
    terminate_playback = 1;

    if (num_buffers == PLAYBACK_COUNT)
        fprintf(stdout, "Audio not detected.\n");

    free(play_buf);
    free(areas);
    return 0;
}

static void *alsa_capture(void *arg) {
    int err;
    short *cap_buf;
    snd_pcm_t *capture_handle = (snd_pcm_t *)arg;
    snd_pcm_sframes_t cap_delay_frames;
    int num_cap, noise_delay_frames;


    cap_buf = calloc(buffer_frames * channels, sizeof(cap_buf[0]));

    pthread_mutex_lock(&latency_test_mutex);
    while (!sine_started) {
        pthread_cond_wait(&sine_start, &latency_test_mutex);
    }
    pthread_mutex_unlock(&latency_test_mutex);

    /* Begin capture. */
    if ((err = snd_pcm_start(capture_handle)) < 0) {
        fprintf(stderr, "cannot start audio interface for use (%s)\n",
                snd_strerror(err));
        exit(1);
    }

    while (!terminate_capture) {
        num_cap = capture_some(capture_handle, cap_buf,
                               buffer_frames, &cap_delay_frames);

        if (num_cap > 0 && (noise_delay_frames = check_for_noise(cap_buf,
                num_cap, channels)) >= 0) {
            struct timeval cap_time;
            unsigned long latency_us;
            gettimeofday(&cap_time, NULL);

            fprintf(stderr, "Found audio\n");
            fprintf(stderr, "Played at %llu %llu, %ld delay\n",
                    (unsigned long long)sine_start_tv.tv_sec,
                    (unsigned long long)sine_start_tv.tv_usec,
                    playback_delay_frames);
            fprintf(stderr, "Capture at %llu %llu, %ld delay sample %d\n",
                    (unsigned long long)cap_time.tv_sec,
                    (unsigned long long)cap_time.tv_usec,
                    cap_delay_frames, noise_delay_frames);

            latency_us = subtract_timevals(&cap_time, &sine_start_tv);
            fprintf(stdout, "Measured Latency: %lu uS\n", latency_us);

            latency_us = (playback_delay_frames + cap_delay_frames -
                    noise_delay_frames) * 1000000 / rate;
            fprintf(stdout, "Reported Latency: %lu uS\n", latency_us);

            // Noise captured, terminate both threads.
            terminate_playback = 1;
            terminate_capture = 1;
        } else {
            // Capture some more buffers after playback thread has terminated.
            if (terminate_playback && capture_count++ < CAPTURE_MORE_COUNT)
                terminate_capture = 1;
        }
    }

    free(cap_buf);
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

    pthread_mutex_init(&latency_test_mutex, NULL);
    pthread_cond_init(&sine_start, NULL);
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
    pthread_mutex_lock(&latency_test_mutex);
    while (!terminate_capture) {
        pthread_cond_wait(&terminate_test, &latency_test_mutex);
    }
    pthread_mutex_unlock(&latency_test_mutex);

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

    if (--loop > 0) {
        if (cras_play_time)
            free(cras_play_time);
        if (cras_cap_time)
            free(cras_cap_time);
        usleep(50000);
        cras_play_time = cras_cap_time = NULL;
        playback_count = 0;

        pthread_mutex_lock(&latency_test_mutex);
        terminate_capture = 0;
        sine_started = 0;
        pthread_mutex_unlock(&latency_test_mutex);
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

void alsa_test_latency(char *play_dev, char *cap_dev)
{
    int err;
    snd_pcm_t *playback_handle;
    snd_pcm_t *capture_handle;

    pthread_t capture_thread;
    pthread_t playback_thread;

    if ((err = snd_pcm_open(&playback_handle, play_dev,
                SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "cannot open audio device %s (%s)\n",
                play_dev, snd_strerror(err));
        exit(1);
    }
    config_pcm_hw_params(playback_handle, rate, channels, format,
            &buffer_frames, &period_size);
    config_pcm_sw_params(playback_handle, start_threshold);

    if ((err = snd_pcm_open(&capture_handle, cap_dev,
                SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf(stderr, "cannot open audio device %s (%s)\n",
                cap_dev, snd_strerror(err));
        exit(1);
    }
    config_pcm_hw_params(capture_handle, rate, channels, format,
            &buffer_frames, &period_size);

    pthread_mutex_init(&latency_test_mutex, NULL);
    pthread_cond_init(&sine_start, NULL);

    pthread_create(&playback_thread, NULL, alsa_play, playback_handle);
    pthread_create(&capture_thread, NULL, alsa_capture, capture_handle);

    pthread_join(capture_thread, NULL);
    pthread_join(playback_thread, NULL);

    snd_pcm_close(playback_handle);
    snd_pcm_close(capture_handle);
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
            buffer_frames = atoi(optarg);
            break;
        case 'c':
            cras_only = 1;
            break;
        case 'i':
            cap_dev = optarg;
            fprintf(stderr, "Assign cap_dev %s\n", cap_dev);
            break;
        case 'n':
            noise_threshold = atoi(optarg);
            break;
        case 'r':
            rate = atoi(optarg);
            break;
        case 'o':
            play_dev = optarg;
            fprintf(stderr, "Assign play_dev %s\n", play_dev);
            break;
        case 'p':
            period_size = atoi(optarg);
            break;
        case 't':
            tty_output_dev = optarg;
            break;
        case 'l':
            loop = atoi(optarg);
            break;
        case 'C':
            cold = 1;
            break;
        case 'P':
            pin_capture_device = atoi(optarg);
            fprintf(stderr,
                "Pinning capture device %d\n",
                pin_capture_device);
            break;
        case 's':
            start_threshold = atoi(optarg);
            break;
        default:
            return 1;
        }
    }

    if (loop && cold) {
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
