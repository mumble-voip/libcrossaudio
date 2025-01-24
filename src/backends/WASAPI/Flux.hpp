// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_WASAPI_FLUX_HPP
#define CROSSAUDIO_SRC_BACKENDS_WASAPI_FLUX_HPP

#include <atomic>
#include <memory>

#include "crossaudio/ErrorCode.h"
#include "crossaudio/Flux.h"

namespace std {
class thread;
}

typedef CrossAudio_ErrorCode ErrorCode;

typedef CrossAudio_FluxConfig FluxConfig;
typedef CrossAudio_FluxFeedback FluxFeedback;

struct IAudioClient3;
struct IMMDevice;

namespace wasapi {
class Engine;

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

#endif
