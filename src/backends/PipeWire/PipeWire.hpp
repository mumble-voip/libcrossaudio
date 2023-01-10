// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_PIPEWIRE_PIPEWIRE_HPP
#define CROSSAUDIO_SRC_BACKENDS_PIPEWIRE_PIPEWIRE_HPP

#include "Library.hpp"

#include "Backend.h"

#include "crossaudio/Direction.h"
#include "crossaudio/ErrorCode.h"
#include "crossaudio/Flux.h"

#include <spa/utils/hook.h>

using FluxConfig   = CrossAudio_FluxConfig;
using FluxFeedback = CrossAudio_FluxFeedback;

struct spa_audio_info_raw;

struct BE_Engine {
	BE_Engine()  = default;
	~BE_Engine() = default;

	pw_thread_loop *threadLoop;
	pw_context *context;
	pw_core *core;

private:
	BE_Engine(const BE_Engine &)            = delete;
	BE_Engine &operator=(const BE_Engine &) = delete;
};

struct BE_Flux {
	BE_Flux()  = default;
	~BE_Flux() = default;

	BE_Engine *engine;
	FluxFeedback feedback;
	spa_hook listener;
	pw_stream *stream;
	uint32_t frameSize;

private:
	BE_Flux(const BE_Flux &)            = delete;
	BE_Flux &operator=(const BE_Flux &) = delete;
};

// Backend API

extern const BE_Impl PipeWire_Impl;

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
