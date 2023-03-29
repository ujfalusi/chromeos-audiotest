# Copyright 2019 The ChromiumOS Authors All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CPPFLAGS += -I$(SRC)
LDLIBS += -lm -lpthread

ALSA_CFLAGS := $(shell $(PKG_CONFIG) --cflags alsa)
ALSA_LIBS := $(shell $(PKG_CONFIG) --libs alsa)



CC_BINARY(alsa_conformance_test/alsa_conformance_test): \
	alsa_conformance_test/alsa_conformance_args.o \
	alsa_conformance_test/alsa_conformance_helper.o \
	alsa_conformance_test/alsa_conformance_mixer.o \
	alsa_conformance_test/alsa_conformance_test.o \
	alsa_conformance_test/alsa_conformance_thread.o \
	alsa_conformance_test/alsa_conformance_timer.o \
	alsa_conformance_test/alsa_conformance_recorder.o \
	alsa_conformance_test/alsa_conformance_debug.o
CC_BINARY(alsa_conformance_test/alsa_conformance_test): \
	CFLAGS += $(ALSA_CFLAGS)
CC_BINARY(alsa_conformance_test/alsa_conformance_test): \
	LDLIBS += $(ALSA_LIBS)
clean: CLEAN(alsa_conformance_test/alsa_conformance_test)
all: CC_BINARY(alsa_conformance_test/alsa_conformance_test)
