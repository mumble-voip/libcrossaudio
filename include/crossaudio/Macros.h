// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_MACROS_H
#define CROSSAUDIO_MACROS_H

#ifdef _MSC_VER
#	define CROSSAUDIO_COMPILER_MSVC
#endif

#define CROSSAUDIO_ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

#define CROSSAUDIO_ZERO_ALLOC(size) (calloc(1, size))

#ifdef CROSSAUDIO_COMPILER_MSVC
#	define CROSSAUDIO_EXPORT __declspec(dllexport)
#else
#	define CROSSAUDIO_EXPORT __attribute__((visibility("default")))
#endif

#endif
