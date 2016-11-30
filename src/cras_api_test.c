/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <pthread.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "cras_client.h"

#define CAPTURE_PERIOD_SIZE 256
#define CAPTURE_PERIOD_COUNT 4

int num_retries = 3;
int wait_sec = 10;

struct stream_in {
    uint32_t                    sample_rate;
    uint32_t                    channels;
    uint32_t                    period_size;
    struct cras_audio_format    *cras_format;
    struct cras_stream_params   *cras_params;
    cras_stream_id_t            stream_id;

    uint32_t                    frames_to_capture;
    uint32_t                    frames_captured;
    pthread_mutex_t             lock;
    pthread_cond_t              cond;
};

static uint32_t get_input_period_size(uint32_t sample_rate)
{
    /*
     * The supported capture sample rates range from 8000 to 48000.
     * We need to use different buffer size when creating CRAS stream
     * so that appropriate latency is maintained.
     */
    if (sample_rate <= 12000)
        return (CAPTURE_PERIOD_SIZE) / 4;
    else if (sample_rate <= 24000)
        return (CAPTURE_PERIOD_SIZE) / 2;
    else
        return CAPTURE_PERIOD_SIZE;
}

void cras_server_error_cb(struct cras_client *client, void *user_arg);

int cras_open(struct cras_client **client)
{
    int rc;

    rc = cras_client_create(client);
    if (rc < 0) {
        fprintf(stderr, "%s: Failed to create CRAS client.\n", __func__);
        return rc;
    }

    rc = cras_client_connect_timeout(*client, 3000);
    if (rc) {
        fprintf(stderr, "%s: Failed to connect to CRAS server, rc %d.\n",
                __func__, rc);
        goto fail;
    }

    rc = cras_client_run_thread(*client);
    if (rc) {
        fprintf(stderr, "%s: Failed to start CRAS client.\n", __func__);
        goto fail;
    }

    rc = cras_client_connected_wait(*client);
    if (rc) {
        fprintf(stderr, "%s: Failed to wait for connected.\n", __func__);
        goto fail;
    }

    cras_client_set_server_error_cb(*client, cras_server_error_cb, NULL);

    return 0;

fail:
    cras_client_destroy(*client);
    return rc;
}

int cras_close(struct cras_client *client)
{
    cras_client_stop(client);
    cras_client_destroy(client);
    return 0;
}

void cras_server_error_cb(struct cras_client *client, void *user_arg)
{
    fprintf(stderr, "Server error!\n");
    cras_close(client);
}

static int in_read_cb(struct cras_client *client,
                      cras_stream_id_t stream_id,
                      uint8_t *captured_samples,
                      uint8_t *playback_samples,
                      unsigned int frames,
                      const struct timespec *captured_time,
                      const struct timespec *playback_time,
                      void *user_arg)
{
    struct stream_in *in = (struct stream_in *)user_arg;

    pthread_mutex_lock(&in->lock);

    in->frames_captured += frames;
    if (in->frames_captured >= in->frames_to_capture)
        pthread_cond_signal(&in->cond);

    pthread_mutex_unlock(&in->lock);

    return frames;
}

static int in_err_cb(struct cras_client *client,
                     cras_stream_id_t stream_id,
                     int error,
                     void *user_arg)
{
    fprintf(stderr, "%s: enter\n", __func__);
    return 0;
}

struct stream_in *open_input_stream(struct cras_client *client,
                                    uint32_t sample_rate, uint32_t channels)
{
    struct stream_in *in;

    printf("%s: sample_rate %d channels %d\n", __func__, sample_rate, channels);

    in = (struct stream_in *)calloc(1, sizeof(struct stream_in));

    in->sample_rate = sample_rate;
    in->channels = channels;
    in->period_size = get_input_period_size(in->sample_rate);

    in->cras_format = cras_audio_format_create(
            SND_PCM_FORMAT_S16_LE,
            in->sample_rate,
            in->channels);
    if (!in->cras_format) {
        fprintf(stderr, "%s: Failed to create CRAS audio format.\n", __func__);
        goto cleanup;
    }

    in->cras_params = cras_client_unified_params_create(
            CRAS_STREAM_INPUT,
            in->period_size,
            CRAS_STREAM_TYPE_DEFAULT,
            0,
            (void *)in,
            in_read_cb,
            in_err_cb,
            in->cras_format);
    if (!in->cras_params) {
        fprintf(stderr, "%s: Failed to create stream params.\n", __func__);
        goto cleanup;
    }

    printf("%s: stream created.\n", __func__);
    return in;

cleanup:
    if (in->cras_format)
        cras_audio_format_destroy(in->cras_format);
    if (in->cras_params)
        cras_client_stream_params_destroy(in->cras_params);
    free(in);
    return NULL;
}

int start_input_stream(struct cras_client *client, struct stream_in *in)
{
    return cras_client_add_stream(client, &in->stream_id, in->cras_params);
}

void close_input_stream(struct cras_client *client, struct stream_in *in)
{
    if (in->stream_id)
        cras_client_rm_stream(client, in->stream_id);
    cras_audio_format_destroy(in->cras_format);
    cras_client_stream_params_destroy(in->cras_params);
    free(in);
}

int cras_test_capture(struct cras_client *client, uint32_t sample_rate,
                      uint32_t channels, uint32_t segment_ms, uint32_t segments)
{
    struct stream_in *in;
    int i, ret = 0, fail_count = 0;
    pthread_condattr_t attr;
    struct timespec abs_ts, wait_ts;

    in = open_input_stream(client, sample_rate, channels);
    if (!in) {
        fprintf(stderr, "%s: Failed to open input stream.\n", __func__);
        return -1;
    }

    in->frames_to_capture = in->sample_rate * segment_ms / 1000;

    pthread_mutex_init(&in->lock, (const pthread_mutexattr_t *)NULL);
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&in->cond, &attr);

    pthread_mutex_lock(&in->lock);

    ret = start_input_stream(client, in);
    if (ret) {
        pthread_mutex_unlock(&in->lock);
        fprintf(stderr, "%s: Failed to start input stream.\n", __func__);
        goto cleanup;
    }

    for (i = 0; i < segments; ++i) {
        in->frames_captured = 0;

        // Wait for captured data.
        if (wait_sec > 0) {
            clock_gettime(CLOCK_MONOTONIC, &abs_ts);
            wait_ts.tv_sec = wait_sec;
            wait_ts.tv_nsec = 0;
            add_timespecs(&abs_ts, &wait_ts);
            ret = pthread_cond_timedwait(&in->cond, &in->lock, &abs_ts);
        } else {
            ret = pthread_cond_wait(&in->cond, &in->lock);
        }
        if (ret) {
            if (ret == ETIMEDOUT) {
                fprintf(stderr, "%s: Failed to receive captured data.\n",
                        __func__);
            } else {
                fprintf(stderr, "%s: Failed to wait for data.\n", __func__);
            }
            if (++fail_count >= num_retries)
                break;
            else
                continue;
        }

        printf("%s: Captured %d frames for segment %d.\n", __func__,
               in->frames_captured, i);
    }

    pthread_mutex_unlock(&in->lock);

cleanup:
    close_input_stream(client, in);
    return ret;
}

int cras_stress_capture(struct cras_client *client)
{
    uint32_t test_sample_rate[] = {
        12345,
        44100,
        48000,
        96000,
        192000,
    };
    uint32_t test_channels[] = {
        1,
        2,
        3,
        4,
        5,
        6,
        7,
        8,
    };
    int i, j;
    int ret;

    for (i = 0; i < ARRAY_SIZE(test_sample_rate); ++i) {
        for (j = 0; j < ARRAY_SIZE(test_channels); ++j) {
            ret = cras_test_capture(client, test_sample_rate[i],
                                    test_channels[j], 20, 10);
            if (ret) {
                fprintf(stderr,
                        "%s: Failed to capture sample_rate %d channels %d.\n",
                        __func__, test_sample_rate[i], test_channels[j]);
                return ret;
            }
        }
    }

    return 0;
}

int main (int argc, char *argv[])
{
    struct cras_client *client;
    int c, i, rc;
    int num_tests = 1;
    struct option long_opt[] =
    {
        { "help",        no_argument,       NULL, 'h' },
        { "num_tests",   required_argument, NULL, 'n' },
        { "num_retries", required_argument, NULL, 'r' },
        { "wait_sec",    required_argument, NULL, 'w' },
        { NULL,          0,                 NULL, 0   }
    };

    while (1) {
        c = getopt_long(argc, argv, "hn:r:w:", long_opt, NULL);
        if (c == -1)
            break;
        switch (c) {
        case 'n':
            num_tests = atoi(optarg);
            break;

        case 'r':
            num_retries = atoi(optarg);
            break;

        case 'w':
            wait_sec = atoi(optarg);
            break;

        case 'h':
            printf("Usage: %s [OPTIONS]\n", argv[0]);
            printf("  --num_tests <num>       Number of test runs, "
                   "default: %d\n", num_tests);
            printf("  --num_retries <num>     Number of retries, "
                   "default: %d\n", num_retries);
            printf("  --wait_sec <num>        Wait seconds for captured data, "
                   "default: %d, use 0 to wait infinitely\n", wait_sec);
            printf("  -h, --help              Print this help and exit\n");
            printf("\n");
            exit(EXIT_SUCCESS);
        }
    }

    rc = cras_open(&client);
    if (rc) {
        fprintf(stderr, "%s: Failed to open CRAS, rc %d.\n", __func__, rc);
        exit(EXIT_FAILURE);
    }

    // Run tests.
    for (i = 0; i < num_tests; ++i) {
        printf("%s: ============\n", __func__);
        printf("%s: TEST PASS %d\n", __func__, i);
        printf("%s: ============\n", __func__);
        rc = cras_stress_capture(client);
        if (rc) {
            fprintf(stderr, "%s: Failed in test pass %d.\n", __func__, i);
            break;
        }
    }

    if (client)
        cras_close(client);

    exit(EXIT_SUCCESS);
}
