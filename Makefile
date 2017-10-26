# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

all: CC_BINARY(src/alsa_api_test) \
     CC_BINARY(src/alsa_helpers) \
     CXX_BINARY(src/audiofuntest) \
     CC_BINARY(src/cras_api_test) \
     CC_BINARY(src/looptest) \
     CC_BINARY(src/loopback_latency) \
     CXX_BINARY(src/test_tones)
