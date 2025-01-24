// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Engine.hpp"

#include "Node.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

#include <sys/soundcard.h>

using namespace oss;

using Direction = CrossAudio_Direction;

Engine::Engine() {
}

Engine::~Engine() {
	stop();
}

ErrorCode Engine::start() {
	if (!m_mixer.open()) {
		return CROSSAUDIO_EC_CONNECT;
	}

	oss_sysinfo info;
	if (!m_mixer.getSysInfo(info)) {
		const int err = errno;

		stop();

		if (err == EINVAL) {
			// Unsupported OSS version, probably older than 4.x.
			return CROSSAUDIO_EC_SYMBOL;
		} else {
			return CROSSAUDIO_EC_GENERIC;
		}
	}

	return CROSSAUDIO_EC_OK;
}

ErrorCode Engine::stop() {
	m_mixer.close();

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
	oss_sysinfo sysInfo;
	if (!m_mixer.getSysInfo(sysInfo)) {
		return nullptr;
	}

	std::map< std::string, Direction > nodeMap;

	for (decltype(sysInfo.numaudios) i = 0; i < sysInfo.numaudios; ++i) {
		oss_audioinfo info{};
		info.dev = i;
		if (!m_mixer.getAudioInfo(info) || info.caps & PCM_CAP_HIDDEN) {
			continue;
		}

		std::string id = info.devnode;
		fixNodeID(id);

		auto &direction = nodeMap[id];

		auto dir = static_cast< uint8_t >(direction);
		if (info.caps & PCM_CAP_INPUT) {
			dir |= CROSSAUDIO_DIR_IN;
		}
		if (info.caps & PCM_CAP_OUTPUT) {
			dir |= CROSSAUDIO_DIR_OUT;
		}

		direction = static_cast< Direction >(dir);
	}

	auto nodes = nodesNew(nodeMap.size());

	for (auto [i, iter] = std::tuple{ std::size_t(0), nodeMap.cbegin() }; iter != nodeMap.cend(); ++i, ++iter) {
		nodes->items[i].id        = strdup(iter->first.data());
		nodes->items[i].name      = strdup(strrchr(iter->first.data(), '/') + 1);
		nodes->items[i].direction = iter->second;
	}

	return nodes;
}

// https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=246231
void Engine::fixNodeID(std::string &str) {
	const auto dotPos = str.find_last_of('.');
	if (dotPos == str.npos) {
		return;
	}

	const auto slashPos = str.find_last_of('/');
	if (slashPos != str.npos && dotPos < slashPos) {
		return;
	}

	// We didn't return, meaning this is not a valid ID/path. Example:
	//
	// - /dev/dsp0.p0
	// - /dev/dsp0.vp0
	// - /dev/dsp0.r0
	// - /dev/dsp0.vr0
	//
	// We need to remove everything from the dot onward.
	str.resize(dotPos);
}
