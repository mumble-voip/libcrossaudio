// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_PULSEAUDIO_PULSEAUDIO_HPP
#define CROSSAUDIO_SRC_BACKENDS_PULSEAUDIO_PULSEAUDIO_HPP

#include "Library.hpp"

#include "Backend.h"

#include "crossaudio/Direction.h"
#include "crossaudio/ErrorCode.h"
#include "crossaudio/Flux.h"
#include "crossaudio/Node.h"

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>

using FluxConfig   = CrossAudio_FluxConfig;
using FluxFeedback = CrossAudio_FluxFeedback;

using Direction = CrossAudio_Direction;
using Node      = CrossAudio_Node;

namespace pulseaudio {
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
		Node(Node &&node);
		Node(const char *name, const char *description, Direction direction);

		std::string name;
		std::string description;
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

	::Node *engineNodesGet();
	ErrorCode engineNodesFree(::Node *nodes);

	ErrorCode start();
	ErrorCode stop();

	std::string defaultInName();
	std::string defaultOutName();
	void fixNameIfMonitor(std::string &name);

	pa_context *m_context;

private:
	Engine(const Engine &)            = delete;
	Engine &operator=(const Engine &) = delete;

	static void serverInfo(pa_context *context, const pa_server_info *info, void *userData);
	static void sinkInfo(pa_context *context, const pa_sink_info *info, int eol, void *userData);
	static void sourceInfo(pa_context *context, const pa_source_info *info, int eol, void *userData);

	static void contextEvent(pa_context *context, pa_subscription_event_type_t type, unsigned int index,
							 void *userData);
	static void contextState(pa_context *context, void *userData);

	std::atomic_flag m_connectComplete;
	pa_threaded_mainloop *m_threadLoop;
	std::string m_name;

	std::mutex m_nodesLock;
	std::string m_defaultInName;
	std::string m_defaultOutName;
	std::map< uint32_t, Node > m_nodes;
	std::unordered_map< std::string, std::string > m_nodeMonitors;
};

class Flux {
public:
	Flux(Engine &engine);
	~Flux();

	const char *nameGet() const;
	ErrorCode nameSet(const char *name);

	ErrorCode start(FluxConfig &config, const FluxFeedback &feedback);
	ErrorCode stop();
	ErrorCode pause(bool on);

private:
	Flux(const Flux &)            = delete;
	Flux &operator=(const Flux &) = delete;

	void processInput(size_t bytes);
	void processOutput(size_t bytes);

	static constexpr pa_channel_map configToMap(const FluxConfig &config);
	static constexpr pa_sample_format translateFormat(CrossAudio_BitFormat format, uint8_t sampleBits);
	static constexpr pa_channel_position translateChannel(CrossAudio_Channel channel);

	Engine &m_engine;
	FluxFeedback m_feedback;

	pa_stream *m_stream;
	std::string m_name;

	uint32_t m_frameSize;
};
} // namespace pulseaudio

// Backend API

extern const BE_Impl PulseAudio_Impl;

#endif
