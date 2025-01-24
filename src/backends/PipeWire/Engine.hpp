// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_PIPEWIRE_ENGINE_HPP
#define CROSSAUDIO_SRC_BACKENDS_PIPEWIRE_ENGINE_HPP

#include "crossaudio/Direction.h"
#include "crossaudio/Engine.h"
#include "crossaudio/ErrorCode.h"
#include "crossaudio/Node.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>

struct pw_context;
struct pw_core;
struct pw_node_info;
struct pw_thread_loop;

typedef CrossAudio_Direction Direction;
typedef CrossAudio_ErrorCode ErrorCode;

typedef CrossAudio_EngineFeedback EngineFeedback;
typedef CrossAudio_Nodes Nodes;

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
		bool advertised;
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

	pw_thread_loop *m_threadLoop;
	pw_context *m_context;
	pw_core *m_core;

	std::unique_ptr< EventManager > m_eventManager;

private:
	Engine(const Engine &)            = delete;
	Engine &operator=(const Engine &) = delete;

	void addNode(uint32_t id);
	void removeNode(uint32_t id);
	void updateNode(const pw_node_info *info);

	EngineFeedback m_feedback;
	std::map< uint32_t, Node > m_nodes;
};
} // namespace pipewire

#endif
