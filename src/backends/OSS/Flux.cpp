// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Flux.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <thread>
#include <type_traits>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

#include <sys/soundcard.h>

using namespace oss;

typedef CrossAudio_FluxData FluxData;

static constexpr auto DEFAULT_NODE    = "/dev/dsp";
static constexpr auto DEFAULT_QUANTUM = 1024;

Flux::Flux() {
}

Flux::~Flux() {
	stop();
}

ErrorCode Flux::start(FluxConfig &config, const FluxFeedback &feedback) {
	if (m_thread) {
		return CROSSAUDIO_EC_INIT;
	}

	m_halt     = false;
	m_config   = config;
	m_feedback = feedback;

	int openMode;
	std::function< void() > threadFunc;

	switch (config.direction) {
		case CROSSAUDIO_DIR_IN:
			openMode   = O_RDONLY;
			threadFunc = [this]() { processInput(); };
			break;
		case CROSSAUDIO_DIR_OUT:
			openMode   = O_WRONLY;
			threadFunc = [this]() { processOutput(); };
			break;
		default:
			return CROSSAUDIO_EC_GENERIC;
	}

	if (config.node && strcmp(config.node, CROSSAUDIO_FLUX_DEFAULT_NODE) != 0) {
		m_fd = open(config.node, openMode, 0);
	} else {
		m_fd = open(DEFAULT_NODE, openMode, 0);
	}

	int value = translateFormat(config.bitFormat, config.sampleBits);
	if (ioctl(m_fd.get(), SNDCTL_DSP_SETFMT, &value) < 0) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	value = config.channels;
	if (ioctl(m_fd.get(), SNDCTL_DSP_CHANNELS, &value) < 0) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	value = config.sampleRate;
	if (ioctl(m_fd.get(), SNDCTL_DSP_SPEED, &value) < 0) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	m_thread = std::make_unique< std::thread >(threadFunc);

	return CROSSAUDIO_EC_OK;
}

ErrorCode Flux::stop() {
	m_halt = true;
	m_pause.clear();
	m_pause.notify_all();

	if (m_thread) {
		m_thread->join();
		m_thread.reset();
	}

	m_fd.close();

	return CROSSAUDIO_EC_OK;
}

ErrorCode Flux::pause(const bool on) {
	if (on) {
		m_pause.test_and_set();
	} else {
		m_pause.clear();
	}

	m_pause.notify_all();

	return CROSSAUDIO_EC_OK;
}

const char *Flux::nameGet() const {
	// TODO: Implement this.
	return nullptr;
}

ErrorCode Flux::nameSet(const char *) {
	// TODO: Implement this.
	return CROSSAUDIO_EC_OK;
}

void Flux::processInput() {
	const uint32_t frameSize = (std::bit_ceil(m_config.sampleBits) / 8) * m_config.channels;

	std::vector< std::byte > buffer(frameSize * DEFAULT_QUANTUM);

	while (!m_halt) {
		const auto bytes = read(m_fd.get(), buffer.data(), buffer.size());
		if (bytes != static_cast< std::decay_t< decltype(bytes) > >(buffer.size())) {
			break;
		}

		FluxData fluxData = { buffer.data(), static_cast< uint32_t >(bytes / frameSize) };
		m_feedback.process(m_feedback.userData, &fluxData);

		if (m_pause.test()) {
			m_pause.wait(true);
		}
	}

	ioctl(m_fd.get(), SNDCTL_DSP_HALT_INPUT, 0);
}

void Flux::processOutput() {
	const uint32_t frameSize = (std::bit_ceil(m_config.sampleBits) / 8) * m_config.channels;

	std::vector< std::byte > buffer(frameSize * DEFAULT_QUANTUM);

	while (!m_halt) {
		FluxData fluxData = { buffer.data(), DEFAULT_QUANTUM };
		m_feedback.process(m_feedback.userData, &fluxData);

		const auto bytes = write(m_fd.get(), buffer.data(), buffer.size());
		if (bytes != static_cast< std::decay_t< decltype(bytes) > >(buffer.size())) {
			break;
		}

		if (m_pause.test()) {
			ioctl(m_fd.get(), SNDCTL_DSP_SILENCE, 0);
			m_pause.wait(true);
			ioctl(m_fd.get(), SNDCTL_DSP_SKIP, 0);
		}
	}

	ioctl(m_fd.get(), SNDCTL_DSP_HALT_OUTPUT, 0);
}

constexpr int Flux::translateFormat(const CrossAudio_BitFormat format, const uint8_t sampleBits) {
	switch (format) {
		default:
		case CROSSAUDIO_BF_NONE:
			break;
		case CROSSAUDIO_BF_INTEGER_SIGNED:
			switch (sampleBits) {
#ifdef AFMT_S8
				case 8:
					return AFMT_S8;
#endif
#ifdef AFMT_S16_NE
				case 16:
					return AFMT_S16_NE;
#endif
#ifdef AFMT_S24_NE
				case 24:
					return AFMT_S24_NE;
#endif
#ifdef AFMT_S32_NE
				case 32:
					return AFMT_S32_NE;
#endif
			}

			break;
		case CROSSAUDIO_BF_INTEGER_UNSIGNED:
			switch (sampleBits) {
#ifdef AFMT_U8
				case 8:
					return AFMT_U8;
#endif
#ifdef AFMT_U16_NE
				case 16:
					return AFMT_U16_NE;
#endif
#ifdef AFMT_U24_NE
				case 24:
					return AFMT_U24_NE;
#endif
#ifdef AFMT_U32_NE
				case 32:
					return AFMT_U32_NE;
#endif
			}

			break;
#ifdef AFMT_FLOAT
		case CROSSAUDIO_BF_FLOAT:
			if (sampleBits == 32) {
				return AFMT_FLOAT;
			}
#endif
	}

	return AFMT_QUERY;
}
