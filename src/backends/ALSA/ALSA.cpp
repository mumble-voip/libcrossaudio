// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "ALSA.hpp"

#include "Backend.h"
#include "Node.h"

#include <algorithm>
#include <bit>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <thread>
#include <vector>

#include <alsa/asoundlib.h>

#define ALSA_ERRBAIL(x) \
	if (x < 0) {        \
		stop();         \
		return false;   \
	}

using namespace alsa;

using Direction = CrossAudio_Direction;
using FluxData  = CrossAudio_FluxData;

static constexpr auto DEFAULT_NODE = "default";

static constexpr snd_pcm_format_t translateFormat(const CrossAudio_BitFormat format, const uint8_t sampleBits);

static auto toImpl(BE_Engine *engine) {
	return reinterpret_cast< Engine * >(engine);
}

static auto toImpl(BE_Flux *flux) {
	return reinterpret_cast< Flux * >(flux);
}

static const char *name() {
	return "ALSA";
}

static const char *version() {
	return snd_asoundlib_version();
}

static ErrorCode init() {
	if (snd_config_update() < 0) {
		return CROSSAUDIO_EC_GENERIC;
	}

	return CROSSAUDIO_EC_OK;
}

static ErrorCode deinit() {
	if (snd_config_update_free_global() < 0) {
		return CROSSAUDIO_EC_GENERIC;
	}

	return CROSSAUDIO_EC_OK;
}

static BE_Engine *engineNew() {
	return reinterpret_cast< BE_Engine * >(new Engine());
}

static ErrorCode engineFree(BE_Engine *engine) {
	delete toImpl(engine);

	return CROSSAUDIO_EC_OK;
}

static ErrorCode engineStart(BE_Engine *engine, const EngineFeedback *) {
	return toImpl(engine)->start();
}

static ErrorCode engineStop(BE_Engine *engine) {
	return toImpl(engine)->stop();
}

static const char *engineNameGet(BE_Engine *engine) {
	return toImpl(engine)->nameGet();
}

static ErrorCode engineNameSet(BE_Engine *engine, const char *name) {
	return toImpl(engine)->nameSet(name);
}

static Nodes *engineNodesGet(BE_Engine *engine) {
	return toImpl(engine)->engineNodesGet();
}

static BE_Flux *fluxNew(BE_Engine *) {
	return reinterpret_cast< BE_Flux * >(new Flux());
}

static ErrorCode fluxFree(BE_Flux *flux) {
	delete toImpl(flux);

	return CROSSAUDIO_EC_OK;
}

static ErrorCode fluxStart(BE_Flux *flux, FluxConfig *config, const FluxFeedback *feedback) {
	return toImpl(flux)->start(*config, feedback ? *feedback : FluxFeedback());
}

static ErrorCode fluxStop(BE_Flux *flux) {
	return toImpl(flux)->stop();
}

static ErrorCode fluxPause(BE_Flux *flux, const bool on) {
	return toImpl(flux)->pause(on);
}

static const char *fluxNameGet(BE_Flux *flux) {
	return toImpl(flux)->nameGet();
}

static ErrorCode fluxNameSet(BE_Flux *flux, const char *name) {
	return toImpl(flux)->nameSet(name);
}

// clang-format off
const BE_Impl ALSA_Impl = {
	name,
	version,

	init,
	deinit,

	engineNew,
	engineFree,
	engineStart,
	engineStop,
	engineNameGet,
	engineNameSet,
	engineNodesGet,

	fluxNew,
	fluxFree,
	fluxStart,
	fluxStop,
	fluxPause,
	fluxNameGet,
	fluxNameSet
};
// clang-format on

Engine::Engine() {
}

Engine::~Engine() {
	stop();
}

ErrorCode Engine::start() {
	return CROSSAUDIO_EC_OK;
}

ErrorCode Engine::stop() {
	return CROSSAUDIO_EC_OK;
}

const char *Engine::nameGet() const {
	return m_name.data();
}

ErrorCode Engine::nameSet(const char *name) {
	m_name = name;

	return CROSSAUDIO_EC_OK;
}

Nodes *Engine::engineNodesGet() {
	void **hints;
	if (snd_device_name_hint(-1, "pcm", &hints) < 0) {
		return nullptr;
	}

	std::map< std::string_view, std::pair< std::string_view, Direction > > nodeMap;

	for (void **hint = hints; *hint; ++hint) {
		char *id   = snd_device_name_get_hint(*hint, "NAME");
		char *dir  = snd_device_name_get_hint(*hint, "IOID");
		char *name = snd_device_name_get_hint(*hint, "DESC");

		auto &pair = nodeMap[id];

		cleanNodeName(name);
		pair.first = name;

		if (!dir) {
			pair.second = CROSSAUDIO_DIR_BOTH;
		} else if (strcmp(dir, "Input") == 0) {
			pair.second = CROSSAUDIO_DIR_IN;
		} else if (strcmp(dir, "Output") == 0) {
			pair.second = CROSSAUDIO_DIR_OUT;
		}

		free(dir);
	}

	snd_device_name_free_hint(hints);

	if (nodeMap.empty()) {
		return nullptr;
	}

	auto nodes = nodesNew(nodeMap.size());

	for (auto [i, iter] = std::tuple{ std::size_t(0), nodeMap.cbegin() }; iter != nodeMap.cend(); ++i, ++iter) {
		nodes->items[i].id        = const_cast< char        *>(iter->first.data());
		nodes->items[i].name      = const_cast< char      *>(iter->second.first.data());
		nodes->items[i].direction = iter->second.second;
	}

	return nodes;
}

void Engine::cleanNodeName(char *name) {
	for (; *name != '\0'; ++name) {
		if (*name == '\n') {
			*name = ' ';
		}
	}
}

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
