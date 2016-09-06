# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES := \
		src/audiofuntest_v2.cc \
		src/connector.cc \
		src/frame_generator.cc \
		src/evaluator.cc \
		src/param_config.cc

LOCAL_MODULE := audiofuntest_v2

include $(BUILD_EXECUTABLE)
