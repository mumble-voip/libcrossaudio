// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Engine.hpp"

#include "Library.hpp"

#include "Node.h"

#include <cstddef>
#include <cstring>
#include <mutex>

using namespace pulseaudio;

Engine::Engine() : m_context(nullptr) {
	if ((m_threadLoop = lib().threaded_mainloop_new())) {
		const auto api = lib().threaded_mainloop_get_api(m_threadLoop);

		auto props = lib().proplist_new();
		lib().proplist_sets(props, PA_PROP_MEDIA_SOFTWARE, "libcrossaudio");

		m_context = lib().context_new_with_proplist(api, nullptr, props);

		lib().proplist_free(props);
	}
}

Engine::~Engine() {
	stop();

	if (m_context) {
		const auto lock = locker();
		lib().context_unref(m_context);
	}

	if (m_threadLoop) {
		lib().threaded_mainloop_free(m_threadLoop);
	}
}

void Engine::lock() {
	if (m_threadLoop) {
		lib().threaded_mainloop_lock(m_threadLoop);
	}
}

void Engine::unlock() {
	if (m_threadLoop) {
		lib().threaded_mainloop_unlock(m_threadLoop);
	}
}

ErrorCode Engine::start(const EngineFeedback &feedback) {
	switch (lib().context_get_state(m_context)) {
		case PA_CONTEXT_UNCONNECTED:
		case PA_CONTEXT_FAILED:
		case PA_CONTEXT_TERMINATED:
			break;
		default:
			return CROSSAUDIO_EC_INIT;
	}

	m_feedback = feedback;

	lib().context_set_state_callback(m_context, contextState, this);
	lib().context_set_subscribe_callback(m_context, contextEvent, this);

	if (lib().context_connect(m_context, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr) < 0) {
		return CROSSAUDIO_EC_CONNECT;
	}

	if (lib().threaded_mainloop_start(m_threadLoop) < 0) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	m_connectComplete.wait(false);

	return CROSSAUDIO_EC_OK;
}

ErrorCode Engine::stop() {
	lock();

	m_nodes.clear();

	if (m_context) {
		lib().context_disconnect(m_context);
	}

	unlock();

	if (m_threadLoop) {
		lib().threaded_mainloop_stop(m_threadLoop);
	}

	return CROSSAUDIO_EC_OK;
}

const char *Engine::nameGet() const {
	return m_name.data();
}

ErrorCode Engine::nameSet(const char *name) {
	m_name = name;

	const auto lock = locker();

	lib().context_set_name(m_context, name, nullptr, nullptr);

	return CROSSAUDIO_EC_OK;
}

Nodes *Engine::engineNodesGet() {
	const std::unique_lock lock(m_nodesLock);

	auto nodes = nodesNew(m_nodes.size());

	size_t i = 0;

	for (const auto &iter : m_nodes) {
		const auto &nodeIn = iter.second;
		auto &nodeOut      = nodes->items[i++];

		nodeOut.id        = strdup(nodeIn.name.data());
		nodeOut.name      = strdup(nodeIn.description.data());
		nodeOut.direction = nodeIn.direction;
	}

	return nodes;
}

std::string Engine::defaultInName() {
	std::unique_lock lock(m_nodesLock);

	return m_defaultInName;
}

std::string Engine::defaultOutName() {
	std::unique_lock lock(m_nodesLock);

	return m_defaultOutName;
}

void Engine::fixNameIfMonitor(std::string &name) {
	std::unique_lock lock(m_nodesLock);

	if (const auto iter = m_nodeMonitors.find(name); iter != m_nodeMonitors.cend()) {
		name = iter->second;
	}
}

void Engine::addNode(const uint32_t index, const char *name, const char *description, Direction direction,
					 const char *monitorName) {
	{
		std::unique_lock lock(m_nodesLock);

		if (monitorName) {
			direction = CROSSAUDIO_DIR_BOTH;
			m_nodeMonitors.emplace(name, monitorName);
		}

		m_nodes.emplace(index, Node(name, description, direction));
	}

	if (m_feedback.nodeAdded) {
		::Node *nodeNotif = nodeNew();

		nodeNotif->id        = strdup(name);
		nodeNotif->name      = strdup(description);
		nodeNotif->direction = direction;

		m_feedback.nodeAdded(m_feedback.userData, nodeNotif);
	}
}

void Engine::removeNode(const uint32_t index) {
	m_nodesLock.lock();

	const auto iter = m_nodes.extract(index);
	if (iter.empty()) {
		m_nodesLock.unlock();
		return;
	}

	const Node &node = iter.mapped();
	if (node.direction == CROSSAUDIO_DIR_BOTH) {
		m_nodeMonitors.erase(node.name);
	}

	m_nodesLock.unlock();

	if (m_feedback.nodeRemoved) {
		::Node *nodeNotif = nodeNew();

		nodeNotif->id        = strdup(node.name.data());
		nodeNotif->name      = strdup(node.description.data());
		nodeNotif->direction = node.direction;

		m_feedback.nodeRemoved(m_feedback.userData, nodeNotif);
	}
}

void Engine::serverInfo(pa_context *, const pa_server_info *info, void *userData) {
	auto &engine = *static_cast< Engine * >(userData);

	std::unique_lock lock(engine.m_nodesLock);

	engine.m_defaultInName  = info->default_source_name;
	engine.m_defaultOutName = info->default_sink_name;
}

void Engine::sinkInfo(pa_context *, const pa_sink_info *info, const int eol, void *userData) {
	if (eol) {
		return;
	}

	auto &engine = *static_cast< Engine * >(userData);

	engine.addNode(info->index, info->name, info->description, CROSSAUDIO_DIR_OUT, info->monitor_source_name);
}

void Engine::sourceInfo(pa_context *, const pa_source_info *info, const int eol, void *userData) {
	if (eol || info->monitor_of_sink != PA_INVALID_INDEX) {
		return;
	}

	auto &engine = *static_cast< Engine * >(userData);

	engine.addNode(info->index, info->name, info->description, CROSSAUDIO_DIR_IN, nullptr);
}

void Engine::contextEvent(pa_context *context, pa_subscription_event_type_t type, unsigned int index, void *userData) {
	auto &engine = *static_cast< Engine * >(userData);

	switch (type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) {
		case PA_SUBSCRIPTION_EVENT_NEW:
			break;
		case PA_SUBSCRIPTION_EVENT_REMOVE:
			engine.removeNode(index);
			[[fallthrough]];
		default:
			return;
	}

	switch (type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
		case PA_SUBSCRIPTION_EVENT_SINK:
			lib().operation_unref(lib().context_get_sink_info_by_index(context, index, sinkInfo, userData));
			break;
		case PA_SUBSCRIPTION_EVENT_SOURCE:
			lib().operation_unref(lib().context_get_source_info_by_index(context, index, sourceInfo, userData));
			break;
	}
}

void Engine::contextState(pa_context *context, void *userData) {
	auto &engine = *static_cast< Engine * >(userData);

	switch (lib().context_get_state(context)) {
		case PA_CONTEXT_READY: {
			const auto mask =
				static_cast< pa_subscription_mask_t >(PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE);
			lib().operation_unref(lib().context_subscribe(context, mask, nullptr, userData));

			lib().operation_unref(lib().context_get_server_info(context, serverInfo, userData));
			lib().operation_unref(lib().context_get_sink_info_list(context, sinkInfo, userData));
			lib().operation_unref(lib().context_get_source_info_list(context, sourceInfo, userData));

			[[fallthrough]];
		}
		case PA_CONTEXT_FAILED:
		case PA_CONTEXT_TERMINATED:
			engine.m_connectComplete.test_and_set();
			engine.m_connectComplete.notify_all();
		default:
			break;
	}
}

Engine::Node::Node(Node &&node)
	: name(std::move(node.name)), description(std::move(node.description)), direction(node.direction) {
}

Engine::Node::Node(const char *name, const char *description, Direction direction)
	: name(name), description(description), direction(direction) {
}
