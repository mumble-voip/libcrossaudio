// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Engine.hpp"

#include "EventManager.hpp"
#include "Library.hpp"

#include "Node.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <spa/utils/dict.h>

#include <pipewire/keys.h>
#include <pipewire/node.h>

using namespace pipewire;

Engine::Engine() : m_threadLoop(nullptr), m_context(nullptr), m_core(nullptr) {
	if ((m_threadLoop = lib().thread_loop_new(nullptr, nullptr))) {
		m_context = lib().context_new(lib().thread_loop_get_loop(m_threadLoop), nullptr, 0);
	}
}

Engine::~Engine() {
	stop();

	if (m_context) {
		const auto lock = locker();
		lib().context_destroy(m_context);
	}

	if (m_threadLoop) {
		lib().thread_loop_destroy(m_threadLoop);
	}
}

void Engine::lock() {
	if (m_threadLoop) {
		lib().thread_loop_lock(m_threadLoop);
	}
}

void Engine::unlock() {
	if (m_threadLoop) {
		lib().thread_loop_unlock(m_threadLoop);
	}
}

ErrorCode Engine::start(const EngineFeedback &feedback) {
	const EventManager::Feedback eventManagerFeedback{ .nodeAdded = [this](const uint32_t id) { addNode(id); },
													   .nodeRemoved = [this](const uint32_t id) { removeNode(id); },
													   .nodeUpdated =
														   [this](const pw_node_info *info) { updateNode(info); } };

	if (m_core) {
		return CROSSAUDIO_EC_INIT;
	}

	if (m_core = lib().context_connect(m_context, nullptr, 0); !m_core) {
		return CROSSAUDIO_EC_CONNECT;
	}

	m_feedback     = feedback;
	m_eventManager = std::make_unique< EventManager >(m_core, eventManagerFeedback);

	if (lib().thread_loop_start(m_threadLoop) < 0) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	return CROSSAUDIO_EC_OK;
}

ErrorCode Engine::stop() {
	lock();

	m_eventManager.reset();

	m_nodes.clear();

	if (m_core) {
		lib().core_disconnect(m_core);
		m_core = nullptr;
	}

	unlock();

	if (m_threadLoop) {
		lib().thread_loop_stop(m_threadLoop);
	}

	return CROSSAUDIO_EC_OK;
}

const char *Engine::nameGet() const {
	if (const auto props = m_core ? lib().core_get_properties(m_core) : lib().context_get_properties(m_context)) {
		return lib().properties_get(props, PW_KEY_APP_NAME);
	}

	return nullptr;
}

ErrorCode Engine::nameSet(const char *name) {
	const spa_dict_item items[] = { { PW_KEY_APP_NAME, name } };
	const spa_dict dict         = SPA_DICT_INIT_ARRAY(items);

	const auto lock = locker();

	auto ret = lib().context_update_properties(m_context, &dict);
	if (ret >= 1 && m_core) {
		ret = lib().core_update_properties(m_core, &dict);
	}

	return ret >= 1 ? CROSSAUDIO_EC_OK : CROSSAUDIO_EC_GENERIC;
}

Nodes *Engine::engineNodesGet() {
	const auto lock = locker();

	auto nodes = nodesNew(m_nodes.size());

	size_t i = 0;

	for (const auto &iter : m_nodes) {
		const auto &nodeIn = iter.second;
		if (!nodeIn.advertised) {
			continue;
		}

		auto &nodeOut = nodes->items[i++];

		nodeOut.id        = strdup(nodeIn.id.data());
		nodeOut.name      = strdup(nodeIn.name.data());
		nodeOut.direction = nodeIn.direction;
	}

	return nodes;
}

void Engine::addNode(const uint32_t id) {
	lock();
	m_nodes.try_emplace(id, Node());
	unlock();
}

void Engine::removeNode(const uint32_t id) {
	lock();
	const auto iter = m_nodes.extract(id);
	unlock();

	if (!iter.empty() && m_feedback.nodeRemoved) {
		const Node &node  = iter.mapped();
		::Node *nodeNotif = nodeNew();

		nodeNotif->id        = strdup(node.id.data());
		nodeNotif->name      = strdup(node.name.data());
		nodeNotif->direction = node.direction;

		m_feedback.nodeRemoved(m_feedback.userData, nodeNotif);
	}
}

void Engine::updateNode(const pw_node_info *info) {
	const auto lock = locker();

	const auto iter = m_nodes.find(info->id);
	if (iter == m_nodes.cend()) {
		return;
	}

	auto &node = iter->second;
	if (node.advertised) {
		return;
	}

	if (node.id.empty()) {
		const char *id = spa_dict_lookup(info->props, PW_KEY_NODE_NAME);
		if (id) {
			node.id = id;
		}
	}

	if (node.name.empty()) {
		const char *name = spa_dict_lookup(info->props, PW_KEY_NODE_DESCRIPTION);
		if (name) {
			node.name = name;
		}
	}

	if (!(info->n_input_ports || info->n_output_ports) || node.id.empty()) {
		// Don't advertise the node if it has no ports or ID.
		return;
	}

	uint8_t direction = CROSSAUDIO_DIR_NONE;
	if (info->n_input_ports > 0) {
		direction |= CROSSAUDIO_DIR_OUT;
	}
	if (info->n_output_ports > 0) {
		direction |= CROSSAUDIO_DIR_IN;
	}

	node.direction = static_cast< Direction >(direction);

	if (m_feedback.nodeAdded) {
		::Node *nodeNotif = nodeNew();

		nodeNotif->id        = strdup(node.id.data());
		nodeNotif->name      = strdup(node.name.data());
		nodeNotif->direction = node.direction;

		m_feedback.nodeAdded(m_feedback.userData, nodeNotif);

		node.advertised = true;
	}
}
