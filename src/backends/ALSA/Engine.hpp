// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_ALSA_ENGINE_HPP
#define CROSSAUDIO_SRC_BACKENDS_ALSA_ENGINE_HPP

#include "crossaudio/ErrorCode.h"
#include "crossaudio/Node.h"

#include <string>

typedef CrossAudio_ErrorCode ErrorCode;

typedef CrossAudio_Nodes Nodes;

namespace alsa {
class Engine {
public:
	Engine();
	~Engine();

	constexpr operator bool() const { return true; }

	const char *nameGet() const;
	ErrorCode nameSet(const char *name);

	Nodes *engineNodesGet();

	ErrorCode start();
	ErrorCode stop();

private:
	Engine(const Engine &)            = delete;
	Engine &operator=(const Engine &) = delete;

	static void cleanNodeName(char *name);

	std::string m_name;
};
} // namespace alsa

#endif
