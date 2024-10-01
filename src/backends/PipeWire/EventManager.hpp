// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_PIPEWIRE_EVENTMANAGER_HPP
#define CROSSAUDIO_SRC_BACKENDS_PIPEWIRE_EVENTMANAGER_HPP

#include <cstdint>
#include <functional>
#include <unordered_map>

#include <spa/utils/hook.h>

struct pw_core;
struct pw_node_info;
struct pw_proxy;
struct pw_registry;

namespace pipewire {
class EventManager {
public:
	struct Feedback {
		std::function< void(uint32_t id) > nodeAdded;
		std::function< void(uint32_t id) > nodeRemoved;
		std::function< void(const pw_node_info *info) > nodeUpdated;
	};

	class Node {
	public:
		Node(uint32_t id, EventManager &manager);
		~Node();

	private:
		Node(const Node &)            = delete;
		Node &operator=(const Node &) = delete;

		pw_proxy *m_proxy;
		spa_hook m_listener;
	};

	EventManager(pw_core *core, const Feedback &feedback);
	~EventManager();

	constexpr auto registry() { return m_registry; }

	void addNode(uint32_t id);
	void removeNode(uint32_t id);
	void updateNode(const pw_node_info *info);

private:
	EventManager(const EventManager &)            = delete;
	EventManager &operator=(const EventManager &) = delete;

	Feedback m_feedback;
	pw_registry *m_registry;
	spa_hook m_listener;
	std::unordered_map< uint32_t, Node > m_nodes;
};
} // namespace pipewire

#endif
