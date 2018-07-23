/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_ALSA_CONFORMANCE_HELPER_H_
#define INCLUDE_ALSA_CONFORMANCE_HELPER_H_

#include <alsa/asoundlib.h>

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

/* Open pcm handle and malloc hw_params.
 * Args:
 *    handle - Filled with a pointer to the opened pcm.
 *    params - Filled with a pointer to the allocated hardware params.
 *    dev_name - Name of device to open. (e.g. hw:0,0)
 *    stream - Alsa stream type, SND_PCM_STREAM_PLAYBACK or
 *             SND_PCM_STREAM_CAPTURE.
 * Returns:
 *    0 on success, negative error on failure.
 */
int alsa_helper_open(snd_pcm_t **handle,
                     snd_pcm_hw_params_t **params,
                     const char *dev_name,
                     snd_pcm_stream_t stream);

/* Close an alsa device, thin wrapper to snd_pcm_close.
 * Args:
 *    handle - The open PCM to configure.
 * Returns:
 *    0 on success, negative error on failure.
 */
int alsa_helper_close(snd_pcm_t *handle);

#endif /* INCLUDE_ALSA_CONFORMANCE_HELPER_H_ */
