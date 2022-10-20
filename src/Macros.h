// Copyright 2022 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_MACROS_H
#define CROSSAUDIO_SRC_MACROS_H

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

#define ZERO_ALLOC(size) (calloc(1, size))

#ifdef COMPILER_MSVC
#	define EXPORT __declspec(dllexport)
#else
#	define EXPORT __attribute__((visibility("default")))
#endif

#endif
