// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_FLUX_H
#define CROSSAUDIO_SRC_FLUX_H

#include "crossaudio/Flux.h"

typedef struct CrossAudio_Engine Engine;
typedef struct CrossAudio_Flux Flux;
typedef struct CrossAudio_FluxConfig FluxConfig;

typedef struct BE_Flux BE_Flux;
typedef struct BE_Impl BE_Impl;

typedef struct CrossAudio_Flux {
	const BE_Impl *beImpl;
	BE_Flux *beData;
	Engine *engine;
} CrossAudio_Flux;

#endif
