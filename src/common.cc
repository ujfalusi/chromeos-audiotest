// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/common.h"

#include <string.h>

#include <memory>

void ParseActiveChannels(const char *arg, std::set<int> *output) {
  std::unique_ptr<char[]> buf(new char[strlen(arg) + 1]);
  strncpy(buf.get(), arg, strlen(arg) + 1);
  char *token;
  for (char *ptr = buf.get(), *saveptr; ; ptr = NULL) {
    token = strtok_r(ptr, ",", &saveptr);
    if (!token)
      break;
    output->insert(atoi(token));
  }
}
