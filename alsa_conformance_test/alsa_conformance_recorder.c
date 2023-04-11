/*
 * Copyright 2018 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <math.h>
#include <string.h>
#include <sys/param.h>

#include "alsa_conformance_recorder.h"
#include "alsa_conformance_timer.h"

#define ARRAY_SIZE 4096

struct alsa_conformance_recorder {
	unsigned long count;
	unsigned long frames;
	struct timespec time;
	double merge_threshold_t;
	double merge_threshold_sz;
	snd_pcm_sframes_t step_median;
	snd_pcm_sframes_t step_counter[ARRAY_SIZE];

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

	struct alsa_conformance_recorder *previous_state;
};

struct alsa_conformance_recorder *
recorder_create(double merge_threshold_t, snd_pcm_sframes_t merge_threshold_sz)
{
	struct alsa_conformance_recorder *recorder;
	recorder = (struct alsa_conformance_recorder *)malloc(
		sizeof(struct alsa_conformance_recorder));
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
	recorder->frames = 0;
	recorder->time.tv_sec = 0;
	recorder->time.tv_nsec = 0;
	recorder->merge_threshold_t = merge_threshold_t;
	recorder->merge_threshold_sz = merge_threshold_sz;
	recorder->step_median = 0;

	memset(recorder->step_counter, 0,
	       ARRAY_SIZE * sizeof(snd_pcm_sframes_t));

	recorder->rate = -1;
	recorder->offset = -1;
	recorder->err = -1;

	recorder->previous_state = (struct alsa_conformance_recorder *)calloc(
		1, sizeof(struct alsa_conformance_recorder));

	recorder->previous_state->previous_state = recorder->previous_state;
	recorder->previous_state->step_min = UINT32_MAX;
	recorder->previous_state->rate = -1;
	recorder->previous_state->offset = -1;
	recorder->previous_state->err = -1;
	recorder->previous_state->merge_threshold_t = merge_threshold_t;
	recorder->previous_state->merge_threshold_sz = merge_threshold_sz;
	recorder->previous_state->step_median = 0;
	memset(recorder->previous_state->step_counter, 0,
	       ARRAY_SIZE * sizeof(snd_pcm_sframes_t));

	return recorder;
}

void recorder_destroy(struct alsa_conformance_recorder *recorder)
{
	free(recorder->previous_state);
	free(recorder);
}

int should_merge(struct alsa_conformance_recorder *recorder,
		 struct timespec time, unsigned long frames)
{
	double time_s;
	subtract_timespec(&time, &recorder->time);
	time_s = timespec_to_s(&time);
	if (time_s < recorder->merge_threshold_t &&
	    frames < recorder->merge_threshold_sz)
		return 1;
	return 0;
}

int recorder_add(struct alsa_conformance_recorder *recorder,
		 struct timespec time, unsigned long frames)
{
	double time_s;
	unsigned long diff = frames;
	int merged = 0;

	if (recorder->count >= 2) {
		diff = frames - recorder->frames;
	}

	if (should_merge(recorder, time, diff)) {
		merged = 1;
		memcpy(recorder, recorder->previous_state,
		       sizeof(struct alsa_conformance_recorder));
	} else {
		memcpy(recorder->previous_state, recorder,
		       sizeof(struct alsa_conformance_recorder));
	}

	time_s = timespec_to_s(&time);
	recorder->count++;
	recorder->time_sum += time_s;
	recorder->time_square_sum += time_s * time_s;
	recorder->frames_sum += frames;
	recorder->frames_square_sum += frames * frames;
	recorder->time_frames_sum += time_s * frames;

	if (recorder->count >= 2) {
		diff = frames - recorder->frames;
		recorder->step_min = MIN(recorder->step_min, diff);
		recorder->step_max = MAX(recorder->step_max, diff);
		recorder->diff_sum += (double)diff;
		recorder->diff_square_sum += (double)diff * diff;
	}
	if (diff < ARRAY_SIZE) {
		recorder->step_counter[diff]++;
	} else {
		fprintf(stderr, "[Notice] frame_diff %ld >= %d.\n", diff,
			ARRAY_SIZE);
	}
	recorder->frames = frames;
	recorder->time = time;
	return merged;
}

/* Compute average and standard deviation of steps. */
void recorder_compute_step(struct alsa_conformance_recorder *recorder)
{
	double tmp;
	if (recorder->count <= 1) {
		fprintf(stderr,
			"Cannot compute step without enough records.\n");
		exit(EXIT_FAILURE);
	}
	recorder->step_average = recorder->diff_sum / (recorder->count - 1);
	tmp = recorder->diff_square_sum / (recorder->count - 1);
	tmp -= recorder->step_average * recorder->step_average;
	recorder->step_standard = sqrt(tmp);
}

snd_pcm_sframes_t get_step_median(struct alsa_conformance_recorder *recorder)
{
	return recorder->step_median;
}

void recorder_compute_step_median(struct alsa_conformance_recorder *recorder)
{
	int cnt = recorder->count / 2;
	int i = 0;
	while (cnt > 0) {
		cnt -= recorder->step_counter[i++];
	}
	recorder->step_median = i - 1;
}

/* Use data in recorder to compute linear regression. */
void recorder_compute_regression(struct alsa_conformance_recorder *recorder)
{
	/* hat(y_i) = b(x_i) + a */
	double time_average;
	double a, b, err;
	double tmp1, tmp2, tmp3;

	if (recorder->count <= 1) {
		fprintf(stderr,
			"Cannot compute regression without enough records.\n");
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

struct alsa_conformance_recorder_list {
	unsigned long count;
	unsigned long size;
	struct alsa_conformance_recorder **array;
};

struct alsa_conformance_recorder_list *recorder_list_create()
{
	struct alsa_conformance_recorder_list *list;
	list = (struct alsa_conformance_recorder_list *)malloc(
		sizeof(struct alsa_conformance_recorder_list));
	if (!list) {
		perror("malloc (alsa_conformance_recorder_list)");
		exit(EXIT_FAILURE);
	}

	list->count = 0;
	list->size = 8;
	list->array = (struct alsa_conformance_recorder **)malloc(
		list->size * sizeof(struct alsa_conformance_recorder *));
	if (!list->array) {
		perror("malloc (alsa_conformance_recorder_list)");
		exit(EXIT_FAILURE);
	}

	return list;
}

void recorder_list_destroy(struct alsa_conformance_recorder_list *list)
{
	int i;
	for (i = 0; i < list->count; i++)
		recorder_destroy(list->array[i]);
	free(list->array);
	free(list);
}

void recorder_list_add_recorder(struct alsa_conformance_recorder_list *list,
				struct alsa_conformance_recorder *recorder)
{
	if (list->count == list->size) {
		list->size *= 2;
		list->array = (struct alsa_conformance_recorder **)realloc(
			list->array,
			list->size *
				sizeof(struct alsa_conformance_recorder *));
		if (!list->array) {
			perror("realloc (alsa_conformance_recorder_list)");
			exit(EXIT_FAILURE);
		}
	}
	list->array[list->count++] = recorder;
}

void recorder_list_print_result(struct alsa_conformance_recorder_list *list)
{
	double rate = 0;
	double rate_min;
	double rate_max;
	double err = 0;
	double err_min;
	double err_max;
	double step = 0;
	unsigned long step_min = UINT32_MAX;
	unsigned long step_max = 0;
	unsigned long points = 0;
	struct alsa_conformance_recorder *recorder;
	int i;

	if (list->count == 0) {
		printf("No record found.\n");
		return;
	}
	for (i = 0; i < list->count; i++) {
		recorder = list->array[i];
		recorder_compute_step_median(recorder);
		recorder_compute_step(recorder);
		recorder_compute_regression(recorder);
		points += recorder->count;
		step += recorder->step_average;
		rate += recorder->rate;
		err += recorder->err;
		if (i == 0) {
			step_min = recorder->step_min;
			step_max = recorder->step_max;
			rate_min = recorder->rate;
			rate_max = recorder->rate;
			err_min = recorder->err;
			err_max = recorder->err;
		} else {
			step_min = MIN(step_min, recorder->step_min);
			step_max = MAX(step_max, recorder->step_max);
			rate_min = MIN(rate_min, recorder->rate);
			rate_max = MAX(rate_max, recorder->rate);
			err_min = MIN(err_min, recorder->err);
			err_max = MAX(err_max, recorder->err);
		}
	}
	printf("number of recorders: %lu\n", list->count);
	printf("number of points: %lu\n", points);
	if (list->count == 1) {
		printf("step average: %lf\n", step);
		printf("step min: %lu\n", step_min);
		printf("step max: %lu\n", step_max);
		printf("step median: %lu\n", recorder->step_median);
		printf("step standard deviation: %lf\n",
		       list->array[0]->step_standard);
		printf("rate: %lf\n", rate);
		printf("rate error: %lf\n", err);
	} else {
		printf("step average: %lf\n", step / list->count);
		printf("step min: %lu\n", step_min);
		printf("step max: %lu\n", step_max);
		printf("rate average: %lf\n", rate / list->count);
		printf("rate min: %lf\n", rate_min);
		printf("rate max: %lf\n", rate_max);
		printf("rate error average: %lf\n", err / list->count);
		printf("rate error min: %lf\n", err_min);
		printf("rate error max: %lf\n", err_max);
	}
}
