// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Engine.hpp"

#include "Device.hpp"
#include "EventManager.hpp"

#include "Node.h"

#include <mmdeviceapi.h>

using namespace wasapi;

ErrorCode Engine::threadInit() {
	switch (CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY)) {
		case S_OK:
		case S_FALSE:
		case RPC_E_CHANGED_MODE:
			return CROSSAUDIO_EC_OK;
		default:
			return CROSSAUDIO_EC_INIT;
	}
}

ErrorCode Engine::threadDeinit() {
	CoUninitialize();

	return CROSSAUDIO_EC_OK;
}

Engine::Engine() : m_enumerator(nullptr) {
	CoCreateGuid(reinterpret_cast< GUID * >(&m_sessionID));

	CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
					 reinterpret_cast< void ** >(&m_enumerator));
}

Engine::~Engine() {
	if (m_enumerator) {
		m_enumerator->Release();
	}
}

ErrorCode Engine::start(const EngineFeedback &feedback) {
	m_feedback = feedback;

	const EventManager::Feedback eventManagerFeedback{
		.nodeAdded = [this](Node *node) { m_feedback.nodeAdded(m_feedback.userData, node); },
		.nodeRemoved = [this](Node *node) { m_feedback.nodeRemoved(m_feedback.userData, node); }
	};

	m_eventManager = std::make_unique< EventManager >(m_enumerator, eventManagerFeedback);

	return CROSSAUDIO_EC_OK;
}

ErrorCode Engine::stop() {
	m_eventManager.reset();

	return CROSSAUDIO_EC_OK;
}

const char *Engine::nameGet() const {
	return m_name.data();
}

ErrorCode Engine::nameSet(const char *name) {
	m_name = name;

	return CROSSAUDIO_EC_OK;
}

Nodes *Engine::engineNodesGet() {
	IMMDeviceCollection *collection;
	if (m_enumerator->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE, &collection) != S_OK) {
		return nullptr;
	}

	UINT count = 0;
	collection->GetCount(&count);

	auto nodes = nodesNew(count);

	for (decltype(count) i = 0; i < count; ++i) {
		IMMDevice *device;
		if (collection->Item(i, &device) != S_OK) {
			continue;
		}

		populateNode(nodes->items[i], *device, nullptr);

		device->Release();
	}

	collection->Release();

	return nodes;
}
