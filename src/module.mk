# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CPPFLAGS += -I$(SRC)/include
LDLIBS += -lm -lpthread

ALSA_CFLAGS := $(shell $(PKG_CONFIG) --cflags alsa)
ALSA_LIBS := $(shell $(PKG_CONFIG) --libs alsa)
CRAS_CFLAGS := $(shell $(PKG_CONFIG) --cflags libcras)
CRAS_LIBS := $(shell $(PKG_CONFIG) --libs libcras)

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

CC_BINARY(src/cras_api_test): \
	src/cras_api_test.o
CC_BINARY(src/cras_api_test): \
	CFLAGS += $(ALSA_CFLAGS) $(CRAS_CFLAGS)
CC_BINARY(src/cras_api_test): \
	LDLIBS += $(ALSA_LIBS) $(CRAS_LIBS)
clean: CC_BINARY(src/cras_api_test)
all: CC_BINARY(src/cras_api_test)


CXX_BINARY(src/audiofuntest): \
	src/audiofuntest.o \
	src/frame_generator.o \
	src/connector.o \
	src/param_config.o \
	src/evaluator.o
CXX_BINARY(src/audiofuntest): \
	CPPFLAGS += -std=c++11 -I$(SRC)
clean: CLEAN(src/audiofuntest)
all: CXX_BINARY(src/audiofuntest)
