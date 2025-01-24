// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_WASAPI_ENGINE_HPP
#define CROSSAUDIO_SRC_BACKENDS_WASAPI_ENGINE_HPP

#include "crossaudio/Engine.h"
#include "crossaudio/ErrorCode.h"
#include "crossaudio/Node.h"

#include <cstdint>
#include <memory>
#include <string>

typedef CrossAudio_ErrorCode ErrorCode;

typedef CrossAudio_EngineFeedback EngineFeedback;
typedef CrossAudio_Nodes Nodes;

struct IMMDeviceEnumerator;

namespace wasapi {
class EventManager;

class Engine {
public:
	struct SessionID {
		uint32_t part1;
		uint16_t part2;
		uint16_t part3;
		uint8_t part4[8];
	};

	static ErrorCode threadInit();
	static ErrorCode threadDeinit();

	Engine();
	~Engine();

	constexpr operator bool() const { return m_enumerator; }

	const char *nameGet() const;
	ErrorCode nameSet(const char *name);

	Nodes *engineNodesGet();

	ErrorCode start(const EngineFeedback &feedback);
	ErrorCode stop();

	std::string m_name;
	SessionID m_sessionID;
	IMMDeviceEnumerator *m_enumerator;

private:
	Engine(const Engine &)            = delete;
	Engine &operator=(const Engine &) = delete;

	EngineFeedback m_feedback;
	std::unique_ptr< EventManager > m_eventManager;
};
} // namespace wasapi

#endif
