// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_SNDIO_FLUX_HPP
#define CROSSAUDIO_SRC_BACKENDS_SNDIO_FLUX_HPP

#include "crossaudio/ErrorCode.h"
#include "crossaudio/Flux.h"

#include <atomic>
#include <cstdint>
#include <memory>

#ifdef __linux__
namespace std {
class thread;
}
#endif

typedef CrossAudio_ErrorCode ErrorCode;

typedef CrossAudio_FluxConfig FluxConfig;
typedef CrossAudio_FluxFeedback FluxFeedback;

struct sio_hdl;
struct sio_par;

namespace sndio {
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

#endif
