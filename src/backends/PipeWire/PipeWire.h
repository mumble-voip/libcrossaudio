// Copyright 2022 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_PIPEWIRE_PIPEWIRE_H
#define CROSSAUDIO_SRC_BACKENDS_PIPEWIRE_PIPEWIRE_H

#include "Library.h"

#include "CrossAudio/Direction.h"
#include "CrossAudio/ErrorCode.h"
#include "CrossAudio/Flux.h"

#include <spa/utils/hook.h>

typedef enum CrossAudio_ErrorCode ErrorCode;

typedef struct CrossAudio_FluxConfig FluxConfig;
typedef struct CrossAudio_FluxFeedback FluxFeedback;

typedef struct spa_audio_info_raw spa_audio_info_raw;

typedef struct BE_Engine {
	pw_thread_loop *threadLoop;
	pw_context *context;
	pw_core *core;
} BE_Engine;

typedef struct BE_Flux {
	BE_Engine *engine;
	FluxFeedback feedback;
	spa_hook listener;
	pw_stream *stream;
	uint32_t frameSize;
} BE_Flux;

// Backend API

static const char *name();
static const char *version();

static ErrorCode init();
static ErrorCode deinit();

static BE_Engine *engineNew();
static ErrorCode engineFree(BE_Engine *engine);
static ErrorCode engineStart(BE_Engine *engine);
static ErrorCode engineStop(BE_Engine *engine);
static const char *engineNameGet(BE_Engine *engine);
static ErrorCode engineNameSet(BE_Engine *engine, const char *name);

static BE_Flux *fluxNew(BE_Engine *engine);
static ErrorCode fluxFree(BE_Flux *flux);
static ErrorCode fluxStart(BE_Flux *flux, FluxConfig *config, const FluxFeedback *feedback);
static ErrorCode fluxStop(BE_Flux *flux);
static const char *fluxNameGet(BE_Flux *flux);
static ErrorCode fluxNameSet(BE_Flux *flux, const char *name);

// Internal functions

static inline void engineLock(BE_Engine *engine);
static inline void engineUnlock(BE_Engine *engine);

static void processInput(void *userData);
static void processOutput(void *userData);

static inline spa_audio_info_raw configToInfo(const FluxConfig *config);

#endif
