/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_ALSA_CONFORMANCE_THREAD_H_
#define INCLUDE_ALSA_CONFORMANCE_THREAD_H_

/* Create device thread object. */
struct dev_thread *dev_thread_create();

/* Destroy device thread object. */
void dev_thread_destroy(struct dev_thread *thread);

/* Set stream type of device, input or output. */
void dev_thread_set_stream(struct dev_thread *thread, snd_pcm_stream_t stream);

/* Set name of device. */
void dev_thread_set_dev_name(struct dev_thread *thread, const char *name);

/* Open device and initialize params. */
void dev_thread_device_open(struct dev_thread *thread);

/* Print device information. */
void dev_thread_print_device_information(struct dev_thread *thread);

#endif /* INCLUDE_ALSA_CONFORMANCE_THREAD_H_ */
