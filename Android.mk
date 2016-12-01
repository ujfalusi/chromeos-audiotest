# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES := \
		src/audiofuntest.cc \
		src/binary_client.cc \
		src/common.cc \
		src/evaluator.cc \
		src/frame_generator.cc

LOCAL_MODULE := audiofuntest

include $(BUILD_EXECUTABLE)
