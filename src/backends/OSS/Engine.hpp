// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_OSS_ENGINE_HPP
#define CROSSAUDIO_SRC_BACKENDS_OSS_ENGINE_HPP

#include "Mixer.hpp"

#include "crossaudio/ErrorCode.h"
#include "crossaudio/Node.h"

#include <string>

typedef CrossAudio_ErrorCode ErrorCode;

typedef CrossAudio_Nodes Nodes;

namespace oss {
class Engine {
public:
	Engine();
	~Engine();

	constexpr operator bool() const { return static_cast< bool >(m_mixer); }

	const char *nameGet() const;
	ErrorCode nameSet(const char *name);

	Nodes *engineNodesGet();

	ErrorCode start();
	ErrorCode stop();

private:
	Engine(const Engine &)            = delete;
	Engine &operator=(const Engine &) = delete;

	static void fixNodeID(std::string &str);

	Mixer m_mixer;
	std::string m_name;
};
} // namespace oss

#endif
