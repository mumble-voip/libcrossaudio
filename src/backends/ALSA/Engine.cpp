// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Engine.hpp"

#include "Node.h"

#include "crossaudio/Direction.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string_view>
#include <tuple>
#include <utility>

#include <alsa/asoundlib.h>

using namespace alsa;

using Direction = CrossAudio_Direction;

Engine::Engine() {
}

Engine::~Engine() {
	stop();
}

ErrorCode Engine::start() {
	return CROSSAUDIO_EC_OK;
}

ErrorCode Engine::stop() {
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
	void **hints;
	if (snd_device_name_hint(-1, "pcm", &hints) < 0) {
		return nullptr;
	}

	std::map< std::string_view, std::pair< std::string_view, Direction > > nodeMap;

	for (void **hint = hints; *hint; ++hint) {
		char *id   = snd_device_name_get_hint(*hint, "NAME");
		char *dir  = snd_device_name_get_hint(*hint, "IOID");
		char *name = snd_device_name_get_hint(*hint, "DESC");

		auto &pair = nodeMap[id];

		cleanNodeName(name);
		pair.first = name;

		if (!dir) {
			pair.second = CROSSAUDIO_DIR_BOTH;
		} else if (strcmp(dir, "Input") == 0) {
			pair.second = CROSSAUDIO_DIR_IN;
		} else if (strcmp(dir, "Output") == 0) {
			pair.second = CROSSAUDIO_DIR_OUT;
		}

		free(dir);
	}

	snd_device_name_free_hint(hints);

	if (nodeMap.empty()) {
		return nullptr;
	}

	auto nodes = nodesNew(nodeMap.size());

	for (auto [i, iter] = std::tuple{ std::size_t(0), nodeMap.cbegin() }; iter != nodeMap.cend(); ++i, ++iter) {
		nodes->items[i].id        = const_cast< char * >(iter->first.data());
		nodes->items[i].name      = const_cast< char * >(iter->second.first.data());
		nodes->items[i].direction = iter->second.second;
	}

	return nodes;
}

void Engine::cleanNodeName(char *name) {
	for (; *name != '\0'; ++name) {
		if (*name == '\n') {
			*name = ' ';
		}
	}
}
