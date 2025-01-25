// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Flux.hpp"

#include <algorithm>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <functional>
#include <vector>

#include <alsa/asoundlib.h>

#define ALSA_ERRBAIL(x) \
	if (x < 0) {        \
		stop();         \
		return false;   \
	}

using namespace alsa;

using FluxData = CrossAudio_FluxData;

static constexpr auto DEFAULT_NODE = "default";

static constexpr snd_pcm_format_t translateFormat(const CrossAudio_BitFormat format, const uint8_t sampleBits);

Flux::Flux() : m_handle(nullptr) {
}

Flux::~Flux() {
	stop();
}

ErrorCode Flux::start(FluxConfig &config, const FluxFeedback &feedback) {
	if (*this) {
		return CROSSAUDIO_EC_INIT;
	}

	snd_pcm_stream_t dir;
	std::function< void() > threadFunc;

	switch (config.direction) {
		case CROSSAUDIO_DIR_IN:
			dir        = SND_PCM_STREAM_CAPTURE;
			threadFunc = [this]() { processInput(); };
			break;
		case CROSSAUDIO_DIR_OUT:
			dir        = SND_PCM_STREAM_PLAYBACK;
			threadFunc = [this]() { processOutput(); };
			break;
		default:
			return CROSSAUDIO_EC_GENERIC;
	}

	const char *nodeID;
	if (config.node && strcmp(config.node, CROSSAUDIO_FLUX_DEFAULT_NODE) != 0) {
		nodeID = config.node;
	} else {
		nodeID = DEFAULT_NODE;
	}

	if (snd_pcm_open(&m_handle, nodeID, dir, SND_PCM_NONBLOCK) < 0) {
		return CROSSAUDIO_EC_GENERIC;
	}

	if (!setParams(config)) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	if (snd_pcm_prepare(m_handle) < 0 || snd_pcm_start(m_handle) < 0) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	m_config   = config;
	m_feedback = feedback;

	m_thread = std::make_unique< std::thread >(threadFunc);

	return CROSSAUDIO_EC_OK;
}

ErrorCode Flux::stop() {
	if (!*this) {
		return CROSSAUDIO_EC_INIT;
	}

	m_halt = true;
	pause(false);

	if (m_thread) {
		m_thread->join();
		m_thread.reset();
	}

	snd_pcm_close(m_handle);
	m_handle = nullptr;

	return CROSSAUDIO_EC_OK;
}

ErrorCode Flux::pause(const bool on) {
	if (!*this) {
		return CROSSAUDIO_EC_INIT;
	}

	// For capture streams snd_pcm_wait() returns immediately when paused (possible ALSA bug?).
	// Our solution is to use std::atomic_flag as an interlock mechanism.
	if (on) {
		snd_pcm_pause(m_handle, 1);
		m_pause.test_and_set();
	} else {
		snd_pcm_pause(m_handle, 0);
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

	std::vector< std::byte > buffer(frameSize * m_quantum);

	while (!m_halt) {
		if (!handleError(snd_pcm_wait(m_handle, SND_PCM_WAIT_IO))) {
			return;
		}

		snd_pcm_sframes_t ret = snd_pcm_avail_update(m_handle);
		while (!m_halt && ret >= m_quantum) {
			ret = snd_pcm_readi(m_handle, buffer.data(), m_quantum);
			if (ret < 0) {
				if (handleError(ret)) {
					break;
				} else {
					return;
				}
			}

			FluxData fluxData = { buffer.data(), static_cast< uint32_t >(ret) };
			m_feedback.process(m_feedback.userData, &fluxData);

			ret = snd_pcm_avail_update(m_handle);
		}

		if (m_pause.test()) {
			m_pause.wait(true);
		}
	}

	snd_pcm_drop(m_handle);
}

void Flux::processOutput() {
	const uint32_t frameSize = (std::bit_ceil(m_config.sampleBits) / 8) * m_config.channels;

	std::vector< std::byte > buffer(frameSize * m_quantum);

	while (!m_halt) {
		if (!handleError(snd_pcm_wait(m_handle, SND_PCM_WAIT_IO))) {
			return;
		}

		snd_pcm_sframes_t ret = snd_pcm_avail_update(m_handle);
		while (!m_halt && ret >= m_quantum) {
			FluxData fluxData = { buffer.data(), m_quantum };
			m_feedback.process(m_feedback.userData, &fluxData);

			if (!fluxData.frames || !fluxData.data) {
				std::fill(buffer.begin(), buffer.end(), std::byte(0));
				fluxData.frames = m_quantum;
			}

			if (!handleError(snd_pcm_writei(m_handle, buffer.data(), fluxData.frames))) {
				return;
			}

			ret = snd_pcm_avail_update(m_handle);
		}

		if (m_pause.test()) {
			m_pause.wait(true);
		}
	}

	snd_pcm_drain(m_handle);
}

bool Flux::setParams(FluxConfig &config) {
	int dir                   = 0;
	unsigned int periods      = 2;
	snd_pcm_uframes_t quantum = config.sampleRate / 100;

	snd_pcm_hw_params_t *hwParams;
	snd_pcm_hw_params_alloca(&hwParams);
	ALSA_ERRBAIL(snd_pcm_hw_params_any(m_handle, hwParams))
	ALSA_ERRBAIL(snd_pcm_hw_params_set_access(m_handle, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED))
	ALSA_ERRBAIL(snd_pcm_hw_params_set_format(m_handle, hwParams, translateFormat(config.bitFormat, config.sampleBits)))
	ALSA_ERRBAIL(snd_pcm_hw_params_set_rate(m_handle, hwParams, config.sampleRate, 0))
	ALSA_ERRBAIL(snd_pcm_hw_params_set_channels(m_handle, hwParams, config.channels))
	ALSA_ERRBAIL(snd_pcm_hw_params_set_period_size_near(m_handle, hwParams, &quantum, &dir))
	ALSA_ERRBAIL(snd_pcm_hw_params_set_periods_near(m_handle, hwParams, &periods, &dir))
	ALSA_ERRBAIL(snd_pcm_hw_params(m_handle, hwParams))

	snd_pcm_sw_params_t *swParams;
	snd_pcm_sw_params_alloca(&swParams);
	ALSA_ERRBAIL(snd_pcm_sw_params_current(m_handle, swParams))
	ALSA_ERRBAIL(snd_pcm_sw_params_set_avail_min(m_handle, swParams, quantum));
	ALSA_ERRBAIL(snd_pcm_sw_params_set_start_threshold(m_handle, swParams, quantum * (periods - 1)));
	ALSA_ERRBAIL(snd_pcm_sw_params_set_stop_threshold(m_handle, swParams, quantum * periods));
	ALSA_ERRBAIL(snd_pcm_sw_params(m_handle, swParams))

	m_quantum = static_cast< uint32_t >(quantum);

	return true;
}

constexpr bool Flux::handleError(const long error) {
	if (error >= 0) {
		return true;
	}

	switch (error) {
		case -EINTR:
		case -EPIPE:
		case -ESTRPIPE:
			return snd_pcm_recover(m_handle, error, 1) >= 0 ? true : false;
		default:
			return false;
	}
}

constexpr snd_pcm_format_t translateFormat(const CrossAudio_BitFormat format, const uint8_t sampleBits) {
	switch (format) {
		default:
		case CROSSAUDIO_BF_NONE:
			break;
		case CROSSAUDIO_BF_INTEGER_SIGNED:
			switch (sampleBits) {
				case 8:
					return SND_PCM_FORMAT_S8;
				case 16:
					return SND_PCM_FORMAT_S16;
				case 24:
					return SND_PCM_FORMAT_S24;
				case 32:
					return SND_PCM_FORMAT_S32;
			}

			break;
		case CROSSAUDIO_BF_INTEGER_UNSIGNED:
			switch (sampleBits) {
				case 8:
					return SND_PCM_FORMAT_U8;
				case 16:
					return SND_PCM_FORMAT_U16;
				case 24:
					return SND_PCM_FORMAT_U24;
				case 32:
					return SND_PCM_FORMAT_U32;
			}

			break;
		case CROSSAUDIO_BF_FLOAT:
			switch (sampleBits) {
				case 32:
					return SND_PCM_FORMAT_FLOAT;
				case 64:
					return SND_PCM_FORMAT_FLOAT64;
			}
	}

	return SND_PCM_FORMAT_UNKNOWN;
}
