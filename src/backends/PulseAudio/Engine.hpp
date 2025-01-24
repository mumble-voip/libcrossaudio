// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_PULSEAUDIO_ENGINE_HPP
#define CROSSAUDIO_SRC_BACKENDS_PULSEAUDIO_ENGINE_HPP

#include "crossaudio/Direction.h"
#include "crossaudio/Engine.h"
#include "crossaudio/ErrorCode.h"
#include "crossaudio/Node.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>

#include <pulse/def.h>

struct pa_context;
struct pa_server_info;
struct pa_sink_info;
struct pa_source_info;
struct pa_threaded_mainloop;

typedef CrossAudio_Direction Direction;
typedef CrossAudio_ErrorCode ErrorCode;

typedef CrossAudio_EngineFeedback EngineFeedback;
typedef CrossAudio_Nodes Nodes;

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

	Nodes *engineNodesGet();

	ErrorCode start(const EngineFeedback &feedback);
	ErrorCode stop();

	std::string defaultInName();
	std::string defaultOutName();
	void fixNameIfMonitor(std::string &name);

	pa_context *m_context;

private:
	Engine(const Engine &)            = delete;
	Engine &operator=(const Engine &) = delete;

	void addNode(uint32_t index, const char *name, const char *description, Direction direction,
				 const char *monitorName);
	void removeNode(uint32_t index);

	static void serverInfo(pa_context *context, const pa_server_info *info, void *userData);
	static void sinkInfo(pa_context *context, const pa_sink_info *info, int eol, void *userData);
	static void sourceInfo(pa_context *context, const pa_source_info *info, int eol, void *userData);

	static void contextEvent(pa_context *context, pa_subscription_event_type_t type, unsigned int index,
							 void *userData);
	static void contextState(pa_context *context, void *userData);

	EngineFeedback m_feedback;

	std::atomic_flag m_connectComplete;
	pa_threaded_mainloop *m_threadLoop;
	std::string m_name;

	std::mutex m_nodesLock;
	std::string m_defaultInName;
	std::string m_defaultOutName;
	std::map< uint32_t, Node > m_nodes;
	std::unordered_map< std::string, std::string > m_nodeMonitors;
};
} // namespace pulseaudio

#endif
