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
	class Locker {
	public:
		Locker(BE_Engine &engine) : m_engine(engine) { m_engine.lock(); }

		~Locker() { m_engine.unlock(); };

	private:
		BE_Engine &m_engine;
	};

	BE_Engine();
	~BE_Engine();

	constexpr operator bool() const { return m_threadLoop && m_context; }

	Locker locker() { return Locker(*this); };

	void lock();
	void unlock();

	const char *nameGet() const;
	ErrorCode nameSet(const char *name);

	ErrorCode start();
	ErrorCode stop();

	pw_thread_loop *m_threadLoop;
	pw_context *m_context;
	pw_core *m_core;

private:
	BE_Engine(const BE_Engine &)            = delete;
	BE_Engine &operator=(const BE_Engine &) = delete;
};

struct BE_Flux {
	BE_Flux(BE_Engine &engine);
	~BE_Flux();

	constexpr operator bool() const { return m_stream; }

	const char *nameGet() const;
	ErrorCode nameSet(const char *name);

	ErrorCode start(FluxConfig &config, const FluxFeedback &feedback);
	ErrorCode stop();

	BE_Engine &m_engine;
	FluxFeedback m_feedback;
	spa_hook m_listener;
	pw_stream *m_stream;
	uint32_t m_frameSize;

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

static void processInput(void *userData);
static void processOutput(void *userData);

static inline spa_audio_info_raw configToInfo(const FluxConfig &config);

#endif
