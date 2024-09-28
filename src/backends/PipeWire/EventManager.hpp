// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_PIPEWIRE_EVENTMANAGER_HPP
#define CROSSAUDIO_SRC_BACKENDS_PIPEWIRE_EVENTMANAGER_HPP

#include <cstdint>
#include <functional>

#include <spa/utils/hook.h>

struct pw_core;
struct pw_node_info;
struct pw_proxy;
struct pw_registry;

namespace pipewire {
class EventManager {
public:
	struct Feedback {
		std::function< void(const pw_node_info *info) > nodeAdded;
		std::function< void(uint32_t id) > nodeRemoved;
	};

	EventManager(pw_core *core, const Feedback &feedback);
	~EventManager();

	constexpr auto &feedback() { return m_feedback; }
	constexpr auto registry() { return m_registry; }

private:
	EventManager(const EventManager &)            = delete;
	EventManager &operator=(const EventManager &) = delete;

	Feedback m_feedback;
	pw_registry *m_registry;
	spa_hook m_registryListener;
};

class NodeInfoData {
public:
	NodeInfoData(EventManager &manager, const uint32_t id);
	~NodeInfoData();

	constexpr auto &manager() { return m_manager; }

private:
	EventManager &m_manager;
	pw_proxy *m_proxy;
	spa_hook m_listener;
};
} // namespace pipewire

#endif
