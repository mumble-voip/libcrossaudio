// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "EventManager.hpp"

#include "Library.hpp"

#include <pipewire/core.h>
#include <pipewire/node.h>

using namespace pipewire;

static constexpr auto NODE_TYPE_ID = "PipeWire:Interface:Node";

static constexpr pw_registry_events eventsRegistry = { PW_VERSION_REGISTRY_EVENTS,
													   [](void *userData, const uint32_t id,
														  const uint32_t /*permissions*/, const char *type,
														  const uint32_t /*version*/, const spa_dict * /*props*/) {
														   if (spa_streq(type, NODE_TYPE_ID)) {
															   auto &manager = *static_cast< EventManager * >(userData);

															   new NodeInfoData(manager, id);
														   }
													   },
													   [](void *userData, const uint32_t id) {
														   auto &manager = *static_cast< EventManager*                       >(userData);

														   manager.feedback().nodeRemoved(id);
													   } };

static constexpr pw_node_events eventsNode = { PW_VERSION_NODE_EVENTS,
											   [](void *userData, const pw_node_info *info) {
												   auto data = static_cast< NodeInfoData*                   >(userData);

												   data->manager().feedback().nodeAdded(info);

												   delete data;
											   },
											   nullptr };

EventManager::EventManager(pw_core *core, const Feedback &feedback)
	: m_feedback(feedback), m_registry(pw_core_get_registry(core, PW_VERSION_REGISTRY, 0)) {
	if (m_registry) {
		pw_registry_add_listener(m_registry, &m_registryListener, &eventsRegistry, this);
	}
}

EventManager::~EventManager() {
	if (m_registry) {
		spa_hook_remove(&m_registryListener);
		lib().proxy_destroy(reinterpret_cast< pw_proxy * >(m_registry));
	}
}

NodeInfoData::NodeInfoData(EventManager &manager, const uint32_t id)
	: m_manager(manager),
	  m_proxy(static_cast< pw_proxy * >(pw_registry_bind(manager.registry(), id, NODE_TYPE_ID, PW_VERSION_NODE, 0))),
	  m_listener() {
	if (m_proxy) {
		lib().proxy_add_object_listener(m_proxy, &m_listener, &eventsNode, this);
	}
}

NodeInfoData::~NodeInfoData() {
	spa_hook_remove(&m_listener);

	if (m_proxy) {
		lib().proxy_destroy(m_proxy);
	}
}
