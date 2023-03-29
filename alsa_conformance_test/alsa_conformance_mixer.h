/*
 * Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_ALSA_CONFORMANCE_MIXER_H_
#define INCLUDE_ALSA_CONFORMANCE_MIXER_H_

#include <alsa/asoundlib.h>

struct alsa_mixer_control {
	// ALSA mixer element.
	snd_mixer_elem_t *elem;
	// Name of the control
	const char *name;
	// Index of the control
	int index;
	// has volume or not
	int has_volume;
	// the maximum volume for this control
	long max_volume_dB;
	// the minimum volume for this control
	long min_volume_dB;
	// the minimum volume range for this control
	long volume_range_min;
	// the maximum volume range for this control
	long volume_range_max;
	struct alsa_mixer_control *prev, *next;
};

struct alsa_mixer {
	// Pointer to the opened alsa mixer.
	snd_mixer_t *mixer;
	struct alsa_mixer_control *input_controls;
	struct alsa_mixer_control *output_controls;
};

/*
 * Create alsa_mixer and fill with mixer with given card name
 * Args:
 *    card_name - soundcard name e.x. "hw:0"
 * Returns:
 *    alsa_mixer pointer, NULL on failure.
 */
struct alsa_mixer *alsa_usb_mixer_create(const char *card_name);
/*
 * Free all resource created by alsa_usb_mixer_create
 * Args:
 *    alsa_mixer - mixer for soundcard
 */
void alsa_usb_mixer_control_destroy(struct alsa_mixer *amixer);

#endif /* INCLUDE_ALSA_CONFORMANCE_MIXER_H_ */
