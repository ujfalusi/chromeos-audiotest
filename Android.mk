# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES := \
		audiofuntest_v2.cc \
		connector.cc \
		frame_generator.cc \
		evaluator.cc \
		param_config.cc

LOCAL_MODULE := audiofuntest_v2

include $(BUILD_EXECUTABLE)
