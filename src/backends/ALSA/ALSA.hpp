// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_ALSA_ALSA_HPP
#define CROSSAUDIO_SRC_BACKENDS_ALSA_ALSA_HPP

#include "crossaudio/ErrorCode.h"
#include "crossaudio/Flux.h"
#include "crossaudio/Node.h"

#include <atomic>
#include <memory>
#include <string>

typedef CrossAudio_FluxConfig FluxConfig;
typedef CrossAudio_FluxFeedback FluxFeedback;

typedef CrossAudio_ErrorCode ErrorCode;
typedef CrossAudio_Node Node;

struct BE_Impl;

namespace std {
class thread;
}

typedef struct _snd_pcm snd_pcm_t;

namespace alsa {
class Engine {
public:
	Engine();
	~Engine();

	constexpr operator bool() const { return true; }

	const char *nameGet() const;
	ErrorCode nameSet(const char *name);

	::Node *engineNodesGet();
	ErrorCode engineNodesFree(::Node *nodes);

	ErrorCode start();
	ErrorCode stop();

private:
	Engine(const Engine &)            = delete;
	Engine &operator=(const Engine &) = delete;

	static void cleanNodeName(char *name);

	std::string m_name;
};

class Flux {
public:
	Flux();
	~Flux();

	constexpr operator bool() const { return m_handle; }

	const char *nameGet() const;
	ErrorCode nameSet(const char *name);

	ErrorCode start(FluxConfig &config, const FluxFeedback &feedback);
	ErrorCode stop();
	ErrorCode pause(bool on);

private:
	Flux(const Flux &)            = delete;
	Flux &operator=(const Flux &) = delete;

	void processInput();
	void processOutput();

	bool setParams(FluxConfig &config);
	constexpr bool handleError(long error);

	FluxConfig m_config;
	FluxFeedback m_feedback;

	snd_pcm_t *m_handle;
	uint32_t m_quantum;

	std::atomic_bool m_halt;
	std::atomic_flag m_pause;
	std::unique_ptr< std::thread > m_thread;
};
} // namespace alsa

// Backend API

extern const BE_Impl ALSA_Impl;

#endif
