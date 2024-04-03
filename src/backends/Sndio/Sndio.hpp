// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_SNDIO_SNDIO_HPP
#define CROSSAUDIO_SRC_BACKENDS_SNDIO_SNDIO_HPP

#include "Library.hpp"

#include "crossaudio/ErrorCode.h"
#include "crossaudio/Flux.h"
#include "crossaudio/Node.h"

#include <atomic>
#include <cstdint>
#include <memory>

typedef CrossAudio_FluxConfig FluxConfig;
typedef CrossAudio_FluxFeedback FluxFeedback;

typedef CrossAudio_ErrorCode ErrorCode;
typedef CrossAudio_Node Node;

struct BE_Impl;

#ifdef __linux__
namespace std {
class thread;
}
#endif

namespace sndio {
class Engine {
public:
	Engine()  = default;
	~Engine() = default;

private:
	Engine(const Engine &)            = delete;
	Engine &operator=(const Engine &) = delete;
};

class Flux {
public:
	Flux();
	~Flux();

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

	static bool configToPar(sio_par &par, const FluxConfig &config);

	FluxConfig m_config;
	FluxFeedback m_feedback;

	sio_hdl *m_handle;
	uint16_t m_quantum;

	std::atomic_bool m_halt;
	std::atomic_flag m_pause;
	std::unique_ptr< std::thread > m_thread;
};
} // namespace sndio

// Backend API

extern const BE_Impl Sndio_Impl;

#endif
