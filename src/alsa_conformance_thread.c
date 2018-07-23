/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/alsa_conformance_helper.h"
#include "include/alsa_conformance_thread.h"

struct dev_thread {
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;

    char *dev_name;
    snd_pcm_stream_t stream;
};

struct dev_thread *dev_thread_create()
{
    struct dev_thread *thread;

    thread = (struct dev_thread*) malloc(sizeof(struct dev_thread));
    if (!thread) {
        perror("malloc (dev_thread)");
        exit(EXIT_FAILURE);
    }

    thread->handle = NULL;
    thread->params = NULL;
    thread->dev_name = NULL;
    return thread;
}

void dev_thread_destroy(struct dev_thread *thread)
{
    assert(thread->handle);
    free(thread->dev_name);
    snd_pcm_hw_params_free(thread->params);
    alsa_helper_close(thread->handle);
    free(thread);
}

void dev_thread_set_stream(struct dev_thread *thread, snd_pcm_stream_t stream)
{
    thread->stream = stream;
}

void dev_thread_set_dev_name(struct dev_thread *thread, const char *name)
{
    free(thread->dev_name);
    thread->dev_name = strdup(name);
}

void dev_thread_device_open(struct dev_thread *thread)
{
    int rc;
    assert(thread->dev_name);
    rc = alsa_helper_open(&thread->handle, &thread->params,
                          thread->dev_name, thread->stream);
    if (rc < 0)
        exit(EXIT_FAILURE);
}

void dev_thread_print_device_information(struct dev_thread *thread)
{
    int rc;
    assert(thread->handle);
    rc = print_device_information(thread->handle, thread->params);
    if (rc < 0)
        exit(EXIT_FAILURE);
}
