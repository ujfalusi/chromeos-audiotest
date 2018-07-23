# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CPPFLAGS += -I$(SRC)
LDLIBS += -lm -lpthread

ALSA_CFLAGS := $(shell $(PKG_CONFIG) --cflags alsa)
ALSA_LIBS := $(shell $(PKG_CONFIG) --libs alsa)
CRAS_CFLAGS := $(shell $(PKG_CONFIG) --cflags libcras)
CRAS_LIBS := $(shell $(PKG_CONFIG) --libs libcras)

CXX_BINARY(src/audiofuntest): \
	src/audiofuntest.o \
	src/common.o \
	src/binary_client.o \
	src/evaluator.o \
	src/generator_player.o \
	src/sample_format.o \
	src/tone_generators.o
CXX_BINARY(src/audiofuntest): \
	CPPFLAGS += -std=c++11
clean: CLEAN(src/audiofuntest)
all: CXX_BINARY(src/audiofuntest)

CXX_BINARY(src/test_tones): \
	src/alsa_client.o \
	src/common.o \
	src/sample_format.o \
	src/test_tones.o \
	src/tone_generators.o
CXX_BINARY(src/test_tones): \
	CPPFLAGS += $(ALSA_CFLAGS) -std=c++11
CXX_BINARY(src/test_tones): \
	LDLIBS += $(ALSA_LIBS)
clean: CLEAN(src/test_tones)
all: CXX_BINARY(src/test_tones)

CC_BINARY(src/looptest): \
	src/libaudiodev.o  \
	src/looptest.o
CC_BINARY(src/looptest): \
	CFLAGS += $(ALSA_CFLAGS)
CC_BINARY(src/looptest): \
	LDLIBS += $(ALSA_LIBS)
clean: CLEAN(src/looptest)
all: CC_BINARY(src/looptest)

CC_BINARY(src/loopback_latency): \
	src/loopback_latency.o
CC_BINARY(src/loopback_latency): \
	CFLAGS += $(ALSA_CFLAGS) $(CRAS_CFLAGS)
CC_BINARY(src/loopback_latency): \
	LDLIBS += $(ALSA_LIBS) $(CRAS_LIBS)
clean: CC_BINARY(src/loopback_latency)
all: CC_BINARY(src/loopback_latency)

CC_BINARY(src/alsa_api_test): \
	src/alsa_api_test.o
CC_BINARY(src/alsa_api_test): \
	CFLAGS += $(ALSA_CFLAGS)
CC_BINARY(src/alsa_api_test): \
	LDLIBS += $(ALSA_LIBS)
clean: CC_BINARY(src/alsa_api_test)
all: CC_BINARY(src/alsa_api_test)

CC_BINARY(src/alsa_helpers): \
	src/alsa_helpers.o
CC_BINARY(src/alsa_helpers): \
	CFLAGS += $(ALSA_CFLAGS)
CC_BINARY(src/alsa_helpers): \
	LDLIBS += $(ALSA_LIBS)
clean: CC_BINARY(src/alsa_helpers)
all: CC_BINARY(src/alsa_helpers)

CC_BINARY(src/cras_api_test): \
	src/cras_api_test.o
CC_BINARY(src/cras_api_test): \
	CFLAGS += $(ALSA_CFLAGS) $(CRAS_CFLAGS)
CC_BINARY(src/cras_api_test): \
	LDLIBS += $(ALSA_LIBS) $(CRAS_LIBS)
clean: CC_BINARY(src/cras_api_test)
all: CC_BINARY(src/cras_api_test)

CC_BINARY(src/alsa_conformance_test): \
	src/alsa_conformance_args.o \
	src/alsa_conformance_helper.o \
	src/alsa_conformance_test.o \
	src/alsa_conformance_thread.o \
	src/alsa_conformance_timer.o \
	src/alsa_conformance_recorder.o
CC_BINARY(src/alsa_conformance_test): \
	CFLAGS += $(ALSA_CFLAGS)
CC_BINARY(src/alsa_conformance_test): \
	LDLIBS += $(ALSA_LIBS)
clean: CC_BINARY(src/alsa_conformance_test)
all: CC_BINARY(src/alsa_conformance_test)
