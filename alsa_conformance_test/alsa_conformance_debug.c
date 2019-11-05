/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#include "alsa_conformance_debug.h"

/* Set true to enable debug mode. Or use --debug flag. */
extern int DEBUG_MODE;

void logger(const char *format, ...)
{
	va_list arg;

	if (!DEBUG_MODE)
		return;

	va_start(arg, format);
	vfprintf(stderr, format, arg);
	va_end(arg);
}
