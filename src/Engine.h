// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_ENGINE_H
#define CROSSAUDIO_SRC_ENGINE_H

#include "crossaudio/Engine.h"

typedef struct CrossAudio_Engine Engine;

typedef struct BE_Engine BE_Engine;
typedef struct BE_Impl BE_Impl;

typedef struct CrossAudio_Engine {
	const BE_Impl *beImpl;
	BE_Engine *beData;
} CrossAudio_Engine;

#endif
