# Copyright 2022 The ChromiumOS Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CPPFLAGS += -I$(SRC)
LDLIBS += -lm -lpthread

ALSA_CFLAGS := $(shell $(PKG_CONFIG) --cflags alsa)
ALSA_LIBS := $(shell $(PKG_CONFIG) --libs alsa)
CRAS_CFLAGS := $(shell $(PKG_CONFIG) --cflags libcras)
CRAS_LIBS := $(shell $(PKG_CONFIG) --libs libcras)

objects = loopback_latency/args.o \
	loopback_latency/common.o \
	loopback_latency/alsa_helper.o \
	loopback_latency/main.o

WITH_CRAS ?= false
ifeq ($(WITH_CRAS),true)
CPPFLAGS += -DWITH_CRAS
objects += loopback_latency/cras_helper.o
endif

CC_BINARY(loopback_latency/loopback_latency): \
	$(objects)
CC_BINARY(loopback_latency/loopback_latency): \
	CFLAGS += $(ALSA_CFLAGS) $(CRAS_CFLAGS)
CC_BINARY(loopback_latency/loopback_latency): \
	LDLIBS += $(ALSA_LIBS) $(CRAS_LIBS)
clean: CLEAN(loopback_latency/loopback_latency)
all: CC_BINARY(loopback_latency/loopback_latency)