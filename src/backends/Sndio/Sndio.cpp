// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Sndio.hpp"

#include "Backend.h"

#include <cstdlib>
#include <cstring>
#include <functional>
#include <thread>
#include <vector>

#include <poll.h>

#include <sndio.h>

using namespace sndio;

static constexpr auto DEFAULT_NODE    = SIO_DEVANY;
static constexpr auto DEFAULT_QUANTUM = 1024;

using FluxData = CrossAudio_FluxData;

static auto toImpl(BE_Engine *engine) {
	return reinterpret_cast< Engine * >(engine);
}

static auto toImpl(BE_Flux *flux) {
	return reinterpret_cast< Flux * >(flux);
}

static const char *name() {
	return "Sndio";
}

static const char *version() {
	return nullptr;
}

static ErrorCode init() {
	// clang-format off
	constexpr std::string_view names[] = {
		"libsndio.so",
		"libsndio.so.7"
	};
	// clang-format on

	ErrorCode ret;

	for (const auto name : names) {
		if ((ret = lib().load(name)) != CROSSAUDIO_EC_LIBRARY) {
			break;
		}
	}

	return ret;
}

static ErrorCode deinit() {
	lib().unload();

	return CROSSAUDIO_EC_OK;
}

static BE_Engine *engineNew() {
	return reinterpret_cast< BE_Engine * >(new Engine());
}

static ErrorCode engineFree(BE_Engine *engine) {
	delete toImpl(engine);

	return CROSSAUDIO_EC_OK;
}

static ErrorCode engineStart(BE_Engine *, const EngineFeedback *) {
	return CROSSAUDIO_EC_OK;
}

static ErrorCode engineStop(BE_Engine *) {
	return CROSSAUDIO_EC_OK;
}

static const char *engineNameGet(BE_Engine *) {
	return nullptr;
}

static ErrorCode engineNameSet(BE_Engine *, const char *) {
	return CROSSAUDIO_EC_OK;
}

static Nodes *engineNodesGet(BE_Engine *) {
	return nullptr;
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
constexpr BE_Impl Sndio_Impl = {
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

Flux::Flux() : m_handle(nullptr), m_quantum(0) {
}

Flux::~Flux() {
	stop();
}

ErrorCode Flux::start(FluxConfig &config, const FluxFeedback &feedback) {
	if (m_handle) {
		return CROSSAUDIO_EC_INIT;
	}

	m_halt     = false;
	m_config   = config;
	m_feedback = feedback;

	unsigned int mode;
	std::function< void() > threadFunc;

	switch (config.direction) {
		case CROSSAUDIO_DIR_IN:
			mode       = SIO_REC;
			threadFunc = [this]() { processInput(); };
			break;
		case CROSSAUDIO_DIR_OUT:
			mode       = SIO_PLAY;
			threadFunc = [this]() { processOutput(); };
			break;
		default:
			return CROSSAUDIO_EC_GENERIC;
	}

	if (config.node && strcmp(config.node, CROSSAUDIO_FLUX_DEFAULT_NODE) != 0) {
		m_handle = lib().open(config.node, mode, 1);
	} else {
		m_handle = lib().open(DEFAULT_NODE, mode, 1);
	}

	if (!m_handle) {
		return CROSSAUDIO_EC_GENERIC;
	}

	sio_par par;
	if (!configToPar(par, config) || !lib().setpar(m_handle, &par) || !lib().getpar(m_handle, &par)) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	m_quantum = par.appbufsz / config.channels;

	if (!lib().start(m_handle)) {
		stop();
		return CROSSAUDIO_EC_GENERIC;
	}

	m_thread = std::make_unique< std::thread >(threadFunc);

	return CROSSAUDIO_EC_OK;
}

ErrorCode Flux::stop() {
	m_halt = true;

	if (m_thread) {
		m_thread->join();
		m_thread.reset();
	}

	if (m_handle) {
		lib().close(m_handle);
		m_handle = nullptr;
	}

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

	std::vector< std::byte > buffer(frameSize * m_quantum);
	std::vector< pollfd > fds(lib().nfds(m_handle));

	while (!m_halt) {
		const int numFds = lib().pollfd(m_handle, fds.data(), POLLIN);
		if (numFds > 0 && poll(fds.data(), numFds, -1) < 0) {
			continue;
		}

		if (lib().revents(m_handle, fds.data()) & POLLHUP) {
			return;
		}

		const auto bytes = lib().read(m_handle, buffer.data(), buffer.size());
		if (bytes != buffer.size()) {
			return;
		}

		FluxData fluxData = { buffer.data(), static_cast< uint32_t >(bytes / frameSize) };
		m_feedback.process(m_feedback.userData, &fluxData);

		if (m_pause.test()) {
			lib().stop(m_handle);
			m_pause.wait(true);
			lib().start(m_handle);
		}
	}
}

void Flux::processOutput() {
	const uint32_t frameSize = (std::bit_ceil(m_config.sampleBits) / 8) * m_config.channels;

	std::vector< std::byte > buffer(frameSize * m_quantum);
	std::vector< pollfd > fds(lib().nfds(m_handle));

	while (!m_halt) {
		const int numFds = lib().pollfd(m_handle, fds.data(), POLLOUT);
		if (numFds > 0 && poll(fds.data(), numFds, -1) < 0) {
			continue;
		}

		if (lib().revents(m_handle, fds.data()) & POLLHUP) {
			return;
		}

		FluxData fluxData = { buffer.data(), m_quantum };
		m_feedback.process(m_feedback.userData, &fluxData);

		const auto bytes = lib().write(m_handle, buffer.data(), buffer.size());
		if (bytes != buffer.size()) {
			return;
		}

		if (m_pause.test()) {
			lib().stop(m_handle);
			m_pause.wait(true);
			lib().start(m_handle);
		}
	}
}

bool Flux::configToPar(sio_par &par, const FluxConfig &config) {
	lib().initpar(&par);

	switch (config.bitFormat) {
		default:
		case CROSSAUDIO_BF_NONE:
			break;
		case CROSSAUDIO_BF_INTEGER_SIGNED:
			par.sig = 1;
			break;
		case CROSSAUDIO_BF_INTEGER_UNSIGNED:
			par.sig = 0;
			break;
		case CROSSAUDIO_BF_FLOAT:
			return false;
	}

	par.appbufsz = DEFAULT_QUANTUM * config.channels;
	par.bits     = config.sampleBits;
	par.bps      = SIO_BPS(par.bits);
	par.rate     = config.sampleRate;
	par.rchan = par.pchan = config.channels;
	par.xrun              = SIO_SYNC;

	return true;
}
