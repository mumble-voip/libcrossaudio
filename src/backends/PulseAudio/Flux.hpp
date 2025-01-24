// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_PULSEAUDIO_FLUX_HPP
#define CROSSAUDIO_SRC_BACKENDS_PULSEAUDIO_FLUX_HPP

#include "crossaudio/ErrorCode.h"
#include "crossaudio/Flux.h"

#include <cstdint>
#include <string>

typedef CrossAudio_ErrorCode ErrorCode;

typedef CrossAudio_FluxConfig FluxConfig;
typedef CrossAudio_FluxFeedback FluxFeedback;

struct pa_stream;

namespace pulseaudio {
class Engine;

class Flux {
public:
	Flux(Engine &engine);
	~Flux();

	const char *nameGet() const;
	ErrorCode nameSet(const char *name);

	ErrorCode start(FluxConfig &config, const FluxFeedback &feedback);
	ErrorCode stop();
	ErrorCode pause(bool on);

private:
	Flux(const Flux &)            = delete;
	Flux &operator=(const Flux &) = delete;

	void processInput(size_t bytes);
	void processOutput(size_t bytes);

	Engine &m_engine;
	FluxFeedback m_feedback;

	pa_stream *m_stream;
	std::string m_name;

	uint32_t m_frameSize;
};
} // namespace pulseaudio

#endif
