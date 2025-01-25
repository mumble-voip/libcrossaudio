// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_ALSA_FLUX_HPP
#define CROSSAUDIO_SRC_BACKENDS_ALSA_FLUX_HPP

#include "crossaudio/ErrorCode.h"
#include "crossaudio/Flux.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

typedef CrossAudio_ErrorCode ErrorCode;

typedef CrossAudio_FluxConfig FluxConfig;
typedef CrossAudio_FluxFeedback FluxFeedback;

typedef struct _snd_pcm snd_pcm_t;

namespace alsa {
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

#endif
