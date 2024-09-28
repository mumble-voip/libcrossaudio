// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_PIPEWIRE_PIPEWIRE_HPP
#define CROSSAUDIO_SRC_BACKENDS_PIPEWIRE_PIPEWIRE_HPP

#include "Backend.h"

#include "crossaudio/Direction.h"
#include "crossaudio/ErrorCode.h"
#include "crossaudio/Flux.h"
#include "crossaudio/Node.h"

#include <map>
#include <memory>
#include <string>

#include <spa/param/audio/raw.h>
#include <spa/utils/hook.h>

using FluxConfig   = CrossAudio_FluxConfig;
using FluxFeedback = CrossAudio_FluxFeedback;

using Direction = CrossAudio_Direction;
using Nodes     = CrossAudio_Nodes;

struct pw_context;
struct pw_core;
struct pw_node_info;
struct pw_stream;
struct pw_thread_loop;

namespace pipewire {
class EventManager;

class Engine {
public:
	class Locker {
	public:
		Locker(Engine &engine) : m_engine(engine) { m_engine.lock(); }
		~Locker() { m_engine.unlock(); };

	private:
		Engine &m_engine;
	};

	struct Node {
		std::string id;
		std::string name;
		Direction direction;
	};

	Engine();
	~Engine();

	constexpr operator bool() const { return m_threadLoop && m_context; }

	Locker locker() { return Locker(*this); };

	void lock();
	void unlock();

	const char *nameGet() const;
	ErrorCode nameSet(const char *name);

	Nodes *engineNodesGet();

	ErrorCode start();
	ErrorCode stop();

	pw_thread_loop *m_threadLoop;
	pw_context *m_context;
	pw_core *m_core;

	std::unique_ptr< EventManager > m_eventManager;

private:
	Engine(const Engine &)            = delete;
	Engine &operator=(const Engine &) = delete;

	void addNode(const pw_node_info *info);
	void removeNode(uint32_t id);

	std::map< uint32_t, Node > m_nodes;
};

class Flux {
public:
	Flux(Engine &engine);
	~Flux();

	constexpr operator bool() const { return m_stream; }

	const char *nameGet() const;
	ErrorCode nameSet(const char *name);

	ErrorCode start(FluxConfig &config, const FluxFeedback &feedback);
	ErrorCode stop();
	ErrorCode pause(bool on);

	Engine &m_engine;
	FluxFeedback m_feedback;
	spa_hook m_listener;
	pw_stream *m_stream;
	uint32_t m_frameSize;

private:
	Flux(const Flux &)            = delete;
	Flux &operator=(const Flux &) = delete;
};
} // namespace pipewire

// Backend API

extern const BE_Impl PipeWire_Impl;

// Internal functions

static void processInput(void *userData);
static void processOutput(void *userData);

static constexpr spa_audio_info_raw configToInfo(const FluxConfig &config);
static constexpr spa_audio_format translateFormat(const CrossAudio_BitFormat format, const uint8_t sampleBits);
static constexpr spa_audio_channel translateChannel(const CrossAudio_Channel channel);

#endif
