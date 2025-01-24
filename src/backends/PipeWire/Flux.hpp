// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_PIPEWIRE_FLUX_HPP
#define CROSSAUDIO_SRC_BACKENDS_PIPEWIRE_FLUX_HPP

#include "crossaudio/ErrorCode.h"
#include "crossaudio/Flux.h"

#include <cstdint>

#include <spa/utils/hook.h>

typedef CrossAudio_ErrorCode ErrorCode;

typedef CrossAudio_FluxConfig FluxConfig;
typedef CrossAudio_FluxFeedback FluxFeedback;

struct pw_stream;

namespace pipewire {
class Engine;

class Flux {
public:
	Flux(Engine &engine);
	~Flux();

	constexpr operator bool() const { return m_stream; }

	const char *nameGet() const;
	ErrorCode nameSet(const char *name);

	ErrorCode start(FluxConfig &config, const FluxFeedback &feedback);
	ErrorCode stop();
	ErrorCode pause(bool on);

	Engine &m_engine;
	FluxFeedback m_feedback;
	spa_hook m_listener;
	pw_stream *m_stream;
	uint32_t m_frameSize;

	static void processInput(void *userData);
	static void processOutput(void *userData);

private:
	Flux(const Flux &)            = delete;
	Flux &operator=(const Flux &) = delete;
};
} // namespace pipewire

#endif
