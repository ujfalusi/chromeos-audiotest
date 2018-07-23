/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <math.h>
#include <sys/param.h>

#include "include/alsa_conformance_recorder.h"
#include "include/alsa_conformance_timer.h"

struct alsa_conformance_recorder {
    unsigned long count;
    unsigned long old_frames;

    double time_sum; /* sum(time) */
    double time_square_sum; /* sum(time * time) */
    double frames_sum; /* sum(frames) */
    double frames_square_sum; /* sum(frames * frames) */
    double time_frames_sum; /* sum(time * frames) */
    double diff_sum; /* sum(frames - old_frames) */
    double diff_square_sum; /* sum((frames - old_frames) ^ 2) */

    unsigned long step_max;
    unsigned long step_min;
    double step_average;
    double step_standard;

    double rate;
    double offset;
    double err;
};

struct alsa_conformance_recorder *recorder_create()
{
    struct alsa_conformance_recorder *recorder;
    recorder = (struct alsa_conformance_recorder*)
               malloc(sizeof(struct alsa_conformance_recorder));
    if (!recorder) {
        perror("malloc (alsa_conformance_recorder)");
        exit(EXIT_FAILURE);
    }

    recorder->count = 0;
    recorder->time_sum = 0;
    recorder->time_square_sum = 0;
    recorder->frames_sum = 0;
    recorder->frames_square_sum = 0;
    recorder->time_frames_sum = 0;
    recorder->step_min = UINT32_MAX;
    recorder->step_max = 0;
    recorder->diff_sum = 0;
    recorder->diff_square_sum = 0;

    recorder->rate = -1;
    recorder->offset = -1;
    recorder->err = -1;
    return recorder;
}

void recorder_destroy(struct alsa_conformance_recorder *recorder)
{
    free(recorder);
}

void recorder_add(struct alsa_conformance_recorder *recorder,
        struct timespec time,
        unsigned long frames)
{
    double time_s;
    unsigned long diff;

    time_s = timespec_to_s(&time);
    recorder->count++;
    recorder->time_sum += time_s;
    recorder->time_square_sum += time_s * time_s;
    recorder->frames_sum += frames;
    recorder->frames_square_sum += frames * frames;
    recorder->time_frames_sum += time_s * frames;

    if (recorder->count >= 2) {
        diff = frames - recorder->old_frames;
        recorder->step_min = MIN(recorder->step_min, diff);
        recorder->step_max = MAX(recorder->step_max, diff);
        recorder->diff_sum += (double)diff;
        recorder->diff_square_sum += (double)diff * diff;
    }
    recorder->old_frames = frames;
}

/* Compute average and standard deviation of steps. */
void recorder_compute_step(struct alsa_conformance_recorder *recorder)
{
    double tmp;
    if (recorder->count <= 1) {
        fprintf(stderr, "Cannot compute step without enough records.\n");
        exit(EXIT_FAILURE);
    }
    recorder->step_average = recorder->diff_sum / (recorder->count - 1);
    tmp = recorder->diff_square_sum / (recorder->count - 1);
    tmp -= recorder->step_average * recorder->step_average;
    recorder->step_standard = sqrt(tmp);
}

/* Use data in recorder to compute linear regression. */
void recorder_compute_regression(struct alsa_conformance_recorder *recorder)
{
    /* hat(y_i) = b(x_i) + a */
    double time_average;
    double a, b, err;
    double tmp1, tmp2, tmp3;

    if (recorder->count <= 1) {
        fprintf(stderr, "Cannot compute regression without enough records.\n");
        exit(EXIT_FAILURE);
    }

    /* b = (n * sum(x * y) - sum(x) * sum(y)) / (n * sum(x ^ 2) - sum(x) ^ 2)
         = (sum(x * y) - avg(x) * sum(y)) / (sum(x ^ 2) - avg(x) * sum(x)) */
    time_average = recorder->time_sum / recorder->count;
    b = recorder->time_frames_sum - time_average * recorder->frames_sum;
    b /= recorder->time_square_sum - time_average * recorder->time_sum;

    /* a = avg(y) - b * avg(x) */
    a = recorder->frames_sum / recorder->count - time_average * b;

    /* tmp1 = sum(y_i ^ 2) */
    tmp1 = recorder->frames_square_sum;

    /* tmp2 = sum(y_i * hat(y_i)) */
    tmp2 = a * recorder->frames_sum + b * recorder->time_frames_sum;

    /* tmp3 = sum(hat(y_i) ^ 2) */
    tmp3 = a * a * recorder->count;
    tmp3 += 2 * a * b * recorder->time_sum;
    tmp3 += b * b * recorder->time_square_sum;

    /* err = sqrt(sum((y_i - hat(y_i)) ^ 2) / n) */
    err = sqrt((tmp1 - 2 * tmp2 + tmp3) / recorder->count);

    recorder->rate = b;
    recorder->offset = a;
    recorder->err = err;
}

void recorder_result(struct alsa_conformance_recorder *recorder)
{
    recorder_compute_step(recorder);
    recorder_compute_regression(recorder);
    printf("records count: %u\n", recorder->count);
    printf("[Step] min: %lu, max: %lu, average: %lf, standard deviation: %lf\n",
            recorder->step_min,
            recorder->step_max,
            recorder->step_average,
            recorder->step_standard);
    printf("[Regression] rate: %lf, err: %lf\n",
            recorder->rate, recorder->err);
}
