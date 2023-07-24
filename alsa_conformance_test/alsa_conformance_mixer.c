/*
 * Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "alsa_conformance_mixer.h"

#include <string.h>

#include "include/utlist.h"
#include "include/util.h"
#include <alsa/mixer.h>

static int alsa_mixer_open(const char *card_name, snd_mixer_t **mixer)
{
	int rc;

	*mixer = NULL;
	rc = snd_mixer_open(mixer, 0);
	if (rc < 0) {
		return rc;
	}
	rc = snd_mixer_attach(*mixer, card_name);
	if (rc < 0) {
		goto fail_after_open;
	}
	rc = snd_mixer_selem_register(*mixer, NULL, NULL);
	if (rc < 0) {
		goto fail_after_open;
	}
	rc = snd_mixer_load(*mixer);
	if (rc < 0) {
		goto fail_after_open;
	}
	return rc;

fail_after_open:
	snd_mixer_close(*mixer);
	*mixer = NULL;
	return rc;
}

static const char *output_mixer_name_filter(const char *mixer_name)
{
	const char *output_mixer_names[] = {
		"Headphone", "Headset", "Headset Earphone", "Speaker",
		"PCM",	     "Master",	"Digital",	    "Speaker Volume"
	};

	for (int i = 0; i < ARRAY_SIZE(output_mixer_names); i++) {
		if (!strcmp(mixer_name, output_mixer_names[i])) {
			return mixer_name;
		}
	}
	return NULL;
}

static const char *input_mixer_name_filter(const char *mixer_name)
{
	const char *input_mixer_names[] = { "Capture", "Digital Capture",
					    "Mic",     "Microphone",
					    "Headset", "Mic Volume" };

	for (int i = 0; i < ARRAY_SIZE(input_mixer_names); i++) {
		if (!strcmp(mixer_name, input_mixer_names[i])) {
			return mixer_name;
		}
	}
	return NULL;
}

static struct alsa_mixer_control *
alsa_mixer_output_control_create(snd_mixer_elem_t *elem)
{
	struct alsa_mixer_control *control;
	control = (struct alsa_mixer_control *)calloc(
		1, sizeof(struct alsa_mixer_control));
	if (!control) {
		perror("alsa_mixer_control_create calloc fail");
		exit(EXIT_FAILURE);
	}

	control->elem = elem;
	control->name = snd_mixer_selem_get_name(elem);
	control->index = snd_mixer_selem_get_index(elem);
	control->has_volume = snd_mixer_selem_has_playback_volume(elem);
	snd_mixer_selem_get_playback_dB_range(elem, &control->min_volume_dB,
					      &control->max_volume_dB);
	snd_mixer_selem_get_playback_volume_range(
		elem, &control->volume_range_min, &control->volume_range_max);

	return control;
}

static struct alsa_mixer_control *
alsa_mixer_input_control_create(snd_mixer_elem_t *elem)
{
	struct alsa_mixer_control *control;

	control = (struct alsa_mixer_control *)calloc(
		1, sizeof(struct alsa_mixer_control));
	if (!control) {
		perror("alsa_mixer_control_create calloc fail");
		exit(EXIT_FAILURE);
	}

	control->elem = elem;
	control->name = snd_mixer_selem_get_name(elem);
	control->has_volume = snd_mixer_selem_has_capture_volume(elem);
	snd_mixer_selem_get_capture_dB_range(elem, &control->min_volume_dB,
					     &control->max_volume_dB);
	snd_mixer_selem_get_capture_volume_range(
		elem, &control->volume_range_min, &control->volume_range_max);

	return control;
}

struct alsa_mixer *alsa_usb_mixer_create(const char *card_name)
{
	int rc;
	struct alsa_mixer *amixer;
	struct alsa_mixer_control *control;
	snd_mixer_elem_t *elem;

	if (!card_name) {
		fprintf(stderr, "card name is null\n");
		return NULL;
	}

	amixer = (struct alsa_mixer *)calloc(1, sizeof(struct alsa_mixer));
	if (!amixer) {
		perror("alsa_usb_mixer_create calloc fail");
		exit(EXIT_FAILURE);
	}

	rc = alsa_mixer_open(card_name, &amixer->mixer);

	if (rc < 0) {
		fprintf(stderr, "alsa_mixer_open fail\n");
		return NULL;
	}
	for (elem = snd_mixer_first_elem(amixer->mixer); elem != NULL;
	     elem = snd_mixer_elem_next(elem)) {
		const char *output_mixer_name = output_mixer_name_filter(
			snd_mixer_selem_get_name(elem));
		if (output_mixer_name &&
		    snd_mixer_selem_has_playback_volume(elem)) {
			control = alsa_mixer_output_control_create(elem);
			DL_APPEND(amixer->output_controls, control);
		}

		const char *input_mixer_name =
			input_mixer_name_filter(snd_mixer_selem_get_name(elem));
		if (input_mixer_name &&
		    snd_mixer_selem_has_capture_volume(elem)) {
			control = alsa_mixer_input_control_create(elem);
			DL_APPEND(amixer->input_controls, control);
		}
	}

	return amixer;
}

void alsa_usb_mixer_control_destroy(struct alsa_mixer *amixer)
{
	struct alsa_mixer_control *elem;

	DL_FOREACH (amixer->output_controls, elem) {
		DL_DELETE(amixer->output_controls, elem);
		free(elem);
	}
	DL_FOREACH (amixer->input_controls, elem) {
		DL_DELETE(amixer->input_controls, elem);
		free(elem);
	}
	if (amixer->mixer) {
		snd_mixer_close(amixer->mixer);
	}
	free(amixer);
}