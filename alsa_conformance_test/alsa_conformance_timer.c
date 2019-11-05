/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "alsa_conformance_timer.h"

struct alsa_api_timer {
	struct timespec total_time;
	struct timespec start_time;
	int is_running;
	unsigned long long count_of_calls;
};

struct alsa_conformance_timer {
	struct alsa_api_timer api_timer[ALSA_API_COUNT];
	int enable; /* If false, timer will not work. */
};

int timespec_after(const struct timespec *a, const struct timespec *b)
{
	return (a->tv_sec > b->tv_sec) ||
	       (a->tv_sec == b->tv_sec && a->tv_nsec > b->tv_nsec);
}

void add_timespec(struct timespec *a, const struct timespec *b)
{
	a->tv_sec += b->tv_sec;
	a->tv_nsec += b->tv_nsec;

	if (a->tv_nsec >= 1000000000L) {
		a->tv_sec++;
		a->tv_nsec -= 1000000000L;
	}
}

void subtract_timespec(struct timespec *a, const struct timespec *b)
{
	assert(!timespec_after(b, a));
	a->tv_sec -= b->tv_sec;
	a->tv_nsec -= b->tv_nsec;

	if (a->tv_nsec < 0) {
		a->tv_sec--;
		a->tv_nsec += 1000000000L;
	}
}

char *timespec_to_str(const struct timespec *time)
{
	char buf[30];
	snprintf(buf, 30, "%" PRIu64 ".%09" PRIu64, (uint64_t)time->tv_sec,
		 (uint64_t)time->tv_nsec);
	return strdup(buf);
}

long long timespec_to_ns(const struct timespec *time)
{
	return time->tv_sec * 1e9 + time->tv_nsec;
}

double timespec_to_s(const struct timespec *time)
{
	return (double)time->tv_sec + (double)time->tv_nsec / 1e9;
}

struct alsa_conformance_timer *conformance_timer_create()
{
	struct alsa_conformance_timer *timer;

	timer = (struct alsa_conformance_timer *)malloc(
		sizeof(struct alsa_conformance_timer));
	if (!timer) {
		perror("malloc (alsa_conformance_timer)");
		exit(EXIT_FAILURE);
	}
	memset(timer, 0, sizeof(*timer));
	timer->enable = true;
	return timer;
}

void conformance_timer_destroy(struct alsa_conformance_timer *timer)
{
	free(timer);
}

void conformance_timer_start(struct alsa_conformance_timer *timer,
			     enum ALSA_API id)
{
	struct alsa_api_timer *api_timer;
	int rc;

	if (!timer->enable)
		return;
	api_timer = &timer->api_timer[id];
	assert(!api_timer->is_running);
	api_timer->is_running = 1;
	rc = clock_gettime(CLOCK_MONOTONIC_RAW, &api_timer->start_time);
	if (rc == -1) {
		perror("clock_gettime");
		exit(EXIT_FAILURE);
	}
}

void conformance_timer_stop(struct alsa_conformance_timer *timer,
			    enum ALSA_API id)
{
	struct alsa_api_timer *api_timer;
	struct timespec end_time;
	int rc;

	if (!timer->enable)
		return;
	rc = clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);
	if (rc == -1) {
		perror("clock_gettime");
		exit(-1);
	}
	api_timer = &timer->api_timer[id];
	assert(api_timer->is_running);
	api_timer->is_running = 0;
	api_timer->count_of_calls++;
	subtract_timespec(&end_time, &api_timer->start_time);
	add_timespec(&api_timer->total_time, &end_time);
}

void conformance_timer_enable(struct alsa_conformance_timer *timer)
{
	timer->enable = true;
}

void conformance_timer_disable(struct alsa_conformance_timer *timer)
{
	timer->enable = false;
}

void api_print_result(enum ALSA_API id, const struct alsa_api_timer *api_timer)
{
	char api_name[MAX_ALSA_API_LENGTH];
	char *time_str;
	double average = -1;
	int i;

	strncpy(api_name, alsa_api_str(id), MAX_ALSA_API_LENGTH);
	for (i = 0; api_name[i] != 0; i++)
		api_name[i] = tolower(api_name[i]);

	time_str = timespec_to_str(&api_timer->total_time);

	if (api_timer->count_of_calls) {
		average = (double)timespec_to_ns(&api_timer->total_time);
		average /= (double)api_timer->count_of_calls;
		average /= 1e9;
	}
	printf("%-25s %20s %20llu %20lf\n", api_name, time_str,
	       api_timer->count_of_calls, average);
	free(time_str);
}

void conformance_timer_print_precision()
{
	struct timespec getres;
	char *time_str;
	clock_getres(CLOCK_MONOTONIC_RAW, &getres);
	time_str = timespec_to_str(&getres);
	printf("precision: %s\n", time_str);
	free(time_str);
}

void conformance_timer_print_result(const struct alsa_conformance_timer *timer)
{
	int i;
	printf("%-25s %20s %20s %20s\n", "", "Total_time(s)", "Counts",
	       "Averages(s)");
	for (i = 0; i < ALSA_API_COUNT; i++)
		api_print_result(i, &timer->api_timer[i]);
	conformance_timer_print_precision();
}
