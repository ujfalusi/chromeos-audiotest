/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_ALSA_CONFORMANCE_HELPER_H_
#define INCLUDE_ALSA_CONFORMANCE_HELPER_H_

#include <alsa/asoundlib.h>
#include "include/alsa_conformance_timer.h"

/* Print device information.
 * Args:
 *    handle - The open PCM to configure.
 *    params - The allocated hardware params object.
 * Prints to stdout:
 *    PCM handle name - Name of PCM device.
 *    PCM type - PCM type. (e.g. HW)
 *    stream - PCM stream type. (PLAYBACK or CAPTURE)
 *    channels range - The range of channels supported by device.
 *    available formats - The available formats supported by device.
 *    rate range - The range of rates supported by device.
 *    available rates - The available rates supported by device.
 *    period size range - The range of period size supported by device.
 *    buffer size range - The range of buffer size supported by device.
 * Returns:
 *    0 on success, negative error on failure.
 * */
int print_device_information(snd_pcm_t *handle, snd_pcm_hw_params_t *params);

/* Print selected device parameters.
 * Args:
 *    handle - The open PCM to configure.
 *    params - The allocated hardware params object which has been set.
 * Prints to stdout:
 *    PCM handle name - Name of PCM device.
 *    PCM type - PCM type. (e.g. HW)
 *    stream - PCM stream type. (PLAYBACK or CAPTURE)
 *    access type - PCM access type. (Should be MMAP_INTERLEAVED)
 *    format - PCM format.
 *    channels - PCM channels count.
 *    rate - PCM rate.
 *    period time - PCM period time in us.
 *    period size - PCM period size in frames.
 *    buffer time - PCM buffer time in us.
 *    buffer size - PCM buffer size in frames.
 * Returns:
 *    0 on success, negative error on failure.
 * */
int print_params(snd_pcm_t *handle, snd_pcm_hw_params_t *params);

/* Open pcm handle and malloc hw_params.
 * Args:
 *    timer - A pointer to timer which records the runtime of ALSA APIs.
 *    handle - Filled with a pointer to the opened pcm.
 *    params - Filled with a pointer to the allocated hardware params.
 *    dev_name - Name of device to open. (e.g. hw:0,0)
 *    stream - Alsa stream type, SND_PCM_STREAM_PLAYBACK or
 *             SND_PCM_STREAM_CAPTURE.
 * Returns:
 *    0 on success, negative error on failure.
 */
int alsa_helper_open(struct alsa_conformance_timer *timer,
                     snd_pcm_t **handle,
                     snd_pcm_hw_params_t **params,
                     const char *dev_name,
                     snd_pcm_stream_t stream);

/* Close an alsa device. A thin wrapper to snd_pcm_close.
 * Args:
 *    handle - The open PCM to configure.
 * Returns:
 *    0 on success, negative error on failure.
 */
int alsa_helper_close(snd_pcm_t *handle);

/* Set hw_params.
 * Args:
 *    timer - A pointer to timer which records the runtime of ALSA APIs.
 *    handle - The open PCM to configure.
 *    params - The allocated hardware params object.
 *    format - Alsa format type.
 *    channels - Number of channels.
 *    rate - Rate pointer, it may be changed if not supported.
 *    period_size - Period size pointer, it may be changed if not supported.
 * Returns:
 *    0 on success, negative error on failure.
 */
int alsa_helper_set_hw_params(struct alsa_conformance_timer *timer,
                              snd_pcm_t *handle,
                              snd_pcm_hw_params_t *params,
                              snd_pcm_format_t format,
                              unsigned int channels,
                              unsigned int *rate,
                              snd_pcm_uframes_t *period_size);

/* Set sw_params with default value.
 * Args:
 *    timer - A pointer to timer which records the runtime of ALSA APIs.
 *    handle - The open PCM to configure.
 * Returns:
 *    0 on success, negative error on failure.
 */
int alsa_helper_set_sw_param(struct alsa_conformance_timer *timer,
                             snd_pcm_t *handle);

/* Prepare an alsa device. A thin wrapper to snd_pcm_prepare.
 * Args:
 *    handle - The open PCM to configure.
 * Returns:
 *    0 on success, negative error on failure.
 */
int alsa_helper_prepare(snd_pcm_t *handle);

/* Starts an alsa device. A thin wrapper to snd_pcm_start.
 * Args:
 *    timer - A pointer to timer which records the runtime of ALSA APIs.
 *    handle - The open PCM to configure.
 * Returns:
 *    0 on success, negative error on failure.
 */
int alsa_helper_start(struct alsa_conformance_timer *timer,
                      snd_pcm_t *handle);

/* Drops an alsa device. A thin wrapper to snd_pcm_drop.
 * Args:
 *    handle - The open PCM to configure.
 * Returns:
 *    0 on success, negative error on failure.
 */
int alsa_helper_drop(snd_pcm_t *handle);

/* Return number of frames ready to be read (capture) / written (playback),
 * a thin wrapper to snd_pcm_avail.
 * Args:
 *    timer - A pointer to timer which records the runtime of ALSA APIs.
 *    handle - The open PCM to configure.
 * Returns:
 *    A positive number of frames ready otherwise a negative error code.
 */
snd_pcm_sframes_t alsa_helper_avail(struct alsa_conformance_timer *timer,
                                    snd_pcm_t *handle);

/* Write samples to pcm using mmap.
 * Args:
 *    handle - The open PCM to configure.
 *    buf - The output buffer which contain samples.
 *    size - The size of output buffer.
 * Returns:
 *    0 on success, negative error on failure.
 */
int alsa_helper_write(snd_pcm_t *handle, uint8_t *buf, snd_pcm_uframes_t size);

/* Read samples from pcm using mmap.
 * Args:
 *    handle - The open PCM to configure.
 *    buf - The input buffer which will be filled with samples.
 *    size - The size of input buffer.
 * Returns:
 *    0 on success, negative error on failure.
 */
int alsa_helper_read(snd_pcm_t *handle, uint8_t *buf, snd_pcm_uframes_t size);

#endif /* INCLUDE_ALSA_CONFORMANCE_HELPER_H_ */
