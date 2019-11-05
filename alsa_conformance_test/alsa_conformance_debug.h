/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_ALSA_CONFORMANCE_DEBUG_H_
#define INCLUDE_ALSA_CONFORMANCE_DEBUG_H_

/* Print debug messages. Only available in debug mode. */
__attribute__((__format__(__printf__, 1, 2)))
void logger(const char *format, ...);

#endif /* INCLUDE_ALSA_CONFORMANCE_DEBUG_H_ */
