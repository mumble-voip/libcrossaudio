// Copyright 2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_WASAPI_WASAPI_HPP
#define CROSSAUDIO_SRC_BACKENDS_WASAPI_WASAPI_HPP

#include "crossaudio/Engine.h"
#include "crossaudio/ErrorCode.h"
#include "crossaudio/Flux.h"
#include "crossaudio/Node.h"

#include <atomic>
#include <memory>
#include <string>

typedef CrossAudio_EngineFeedback EngineFeedback;

typedef CrossAudio_FluxConfig FluxConfig;
typedef CrossAudio_FluxFeedback FluxFeedback;

typedef CrossAudio_ErrorCode ErrorCode;
typedef CrossAudio_Nodes Nodes;

struct BE_Impl;

namespace std {
class thread;
}

struct IAudioClient3;
struct IMMDevice;
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

class Flux {
public:
	Flux(Engine &engine);
	~Flux();

	constexpr operator bool() const { return m_event; }

	const char *nameGet() const;
	ErrorCode nameSet(const char *name);

	ErrorCode start(FluxConfig &config, const FluxFeedback &feedback);
	ErrorCode stop();
	ErrorCode pause(bool on);

	void processInput();
	void processOutput();

	Engine &m_engine;
	FluxFeedback m_feedback;

	std::atomic_bool m_halt;
	IMMDevice *m_device;
	IAudioClient3 *m_client;
	void *m_event;

	std::unique_ptr< std::thread > m_thread;

private:
	Flux(const Flux &)            = delete;
	Flux &operator=(const Flux &) = delete;
};
} // namespace wasapi

// Backend API

extern "C" {
extern const BE_Impl WASAPI_Impl;
}

#endif
