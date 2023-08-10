# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

all: CC_BINARY(src/alsa_api_test) \
     CC_BINARY(alsa_conformance_test/alsa_conformance_test) \
     CC_BINARY(src/alsa_helpers) \
     CXX_BINARY(src/audiofuntest) \
     CC_BINARY(src/cras_api_test) \
     CC_BINARY(src/looptest) \
     CC_BINARY(loopback_latency/loopback_latency) \
     CC_BINARY(teensy_latency_test/teensy_latency_test) \
     CXX_BINARY(src/test_tones) \
     script \
     udev

alsa_conformance_test: CC_BINARY(alsa_conformance_test/alsa_conformance_test)


loopback_latency: CC_BINARY(loopback_latency/loopback_latency)

teensy_latency_test: CC_BINARY(teensy_latency_test/teensy_latency_test)

script:
	mkdir -p $(OUT)/script
	install -m 644 $(SRC)/script/* $(OUT)/script

udev:
	mkdir -p $(OUT)/udev
	install -m 644 $(SRC)/teensy_latency_test/49-teensy.rules $(OUT)/udev
