# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

LDLIBS += -lm -lpthread

ALSA_CFLAGS := $(shell $(PKG_CONFIG) --cflags alsa)
ALSA_LIBS := $(shell $(PKG_CONFIG) --libs alsa)
FFTW_CFLAGS := $(shell $(PKG_CONFIG) --cflags fftw3)
FFTW_LIBS := $(shell $(PKG_CONFIG) --libs fftw3)
CRAS_CFLAGS := $(shell $(PKG_CONFIG) --cflags libcras)
CRAS_LIBS := $(shell $(PKG_CONFIG) --libs libcras)

CXX_BINARY(audiofuntest): $(filter \
	alsa_client.o \
	audiofuntest.o \
	tone_generators.o \
	,$(CXX_OBJECTS))
CXX_BINARY(audiofuntest): \
	CPPFLAGS += $(ALSA_CFLAGS) $(FFTW_CFLAGS)
CXX_BINARY(audiofuntest): \
	LDLIBS += $(ALSA_LIBS) $(FFTW_LIBS)
clean: CLEAN(audiofuntest)
all: CXX_BINARY(audiofuntest)

CXX_BINARY(test_tones): $(filter \
	alsa_client.o \
	test_tones.o \
	tone_generators.o \
	,$(CXX_OBJECTS))
CXX_BINARY(test_tones): \
	CPPFLAGS += $(ALSA_CFLAGS)
CXX_BINARY(test_tones): \
	LDLIBS += $(ALSA_LIBS)
clean: CLEAN(test_tones)
all: CXX_BINARY(test_tones)

CC_BINARY(looptest): $(filter \
	libaudiodev.o  \
	looptest.o \
	,$(C_OBJECTS))
CC_BINARY(looptest): \
	CFLAGS += $(ALSA_CFLAGS)
CC_BINARY(looptest): \
	LDLIBS += $(ALSA_LIBS)
clean: CLEAN(looptest)
all: CC_BINARY(looptest)

CC_BINARY(loopback_latency): $(filter \
	loopback_latency.o \
	,$(C_OBJECTS))
CC_BINARY(loopback_latency): \
	CFLAGS += $(ALSA_CFLAGS) $(CRAS_CFLAGS)
CC_BINARY(loopback_latency): \
	LDLIBS += $(ALSA_LIBS) $(CRAS_LIBS)
clean: CC_BINARY(loopback_latency)
all: CC_BINARY(loopback_latency)

CC_BINARY(alsa_api_test): $(filter \
	alsa_api_test.o \
	,$(C_OBJECTS))
CC_BINARY(alsa_api_test): \
	CFLAGS += $(ALSA_CFLAGS)
CC_BINARY(alsa_api_test): \
	LDLIBS += $(ALSA_LIBS)
clean: CC_BINARY(alsa_api_test)
all: CC_BINARY(alsa_api_test)
