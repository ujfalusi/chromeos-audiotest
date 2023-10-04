# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

CPPFLAGS += -I$(SRC)
LDLIBS += -lm -lpthread

ALSA_CFLAGS := $(shell $(PKG_CONFIG) --cflags alsa)
ALSA_LIBS := $(shell $(PKG_CONFIG) --libs alsa)
CRAS_CFLAGS := $(shell $(PKG_CONFIG) --cflags libcras)
CRAS_LIBS := $(shell $(PKG_CONFIG) --libs libcras)

objects = latency_libraries/alsa_helper.o \
	latency_libraries/args.o \
	latency_libraries/common.o \
	latency_libraries/dolphin.o

ifeq ($(WITH_CRAS),true)
CPPFLAGS += -DWITH_CRAS
objects += latency_libraries/cras_helper.o
endif

CC_LIBRARY(latency_libraries/latency_libraries): \
	$(objects)
CC_LIBRARY(latency_libraries/latency_libraries): \
	CFLAGS += $(ALSA_CFLAGS) $(CRAS_CFLAGS)
CC_BINCC_LIBRARYARY(latency_libraries/latency_libraries): \
	LDLIBS += $(ALSA_LIBS) $(CRAS_LIBS)
clean: CLEAN(latency_libraries/latency_libraries)
all: CC_LIBRARY(latency_libraries/latency_libraries)
