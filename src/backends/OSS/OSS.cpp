// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "OSS.hpp"

#include "Backend.h"
#include "Node.h"

#include <bit>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

#include <sys/soundcard.h>

using namespace oss;

using Direction = CrossAudio_Direction;
using FluxData  = CrossAudio_FluxData;

static constexpr auto DEFAULT_NODE    = "/dev/dsp";
static constexpr auto DEFAULT_QUANTUM = 1024;

static auto toImpl(BE_Engine *engine) {
	return reinterpret_cast< Engine * >(engine);
}

static auto toImpl(BE_Flux *flux) {
	return reinterpret_cast< Flux * >(flux);
}

static FileDescriptor openMixer() {
	return open("/dev/mixer", O_RDONLY, 0);
}

static bool getSysInfo(oss_sysinfo &info, const FileDescriptor &fd) {
	return ioctl(fd.get(), SNDCTL_SYSINFO, &info) >= 0;
}

static bool getAudioInfo(oss_audioinfo &info, const FileDescriptor &fd) {
	return ioctl(fd.get(), SNDCTL_AUDIOINFO, &info) >= 0;
}

static const char *name() {
	return "OSS";
}

static const char *version() {
	const auto fd = openMixer();
	if (!fd) {
		return nullptr;
	}

	oss_sysinfo info;
	if (!getSysInfo(info, fd)) {
		return nullptr;
	}

	// - Length of "version" (31)
	// - Length of "product" (31)
	// - Space and parentheses (3)
	// - NUL character (1)
	static char versionString[66];

	if (info.product[0] != '\0') {
		snprintf(versionString, sizeof(versionString), "%s (%s)", info.version, info.product);
	} else {
		strncpy(versionString, info.version, sizeof(versionString));
	}

	return versionString;
}

static ErrorCode init() {
	return CROSSAUDIO_EC_OK;
}

static ErrorCode deinit() {
	return CROSSAUDIO_EC_OK;
}

static BE_Engine *engineNew() {
	return reinterpret_cast< BE_Engine * >(new Engine());
}

static ErrorCode engineFree(BE_Engine *engine) {
	delete toImpl(engine);

	return CROSSAUDIO_EC_OK;
}

static ErrorCode engineStart(BE_Engine *engine) {
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
const BE_Impl OSS_Impl = {
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
	m_fd = openMixer();
	if (!m_fd) {
		return CROSSAUDIO_EC_CONNECT;
	}

	oss_sysinfo info;
	if (!getSysInfo(info, m_fd)) {
		const int err = errno;

		stop();

		if (err == EINVAL) {
			// Unsupported OSS version, probably older than 4.x.
			return CROSSAUDIO_EC_SYMBOL;
		} else {
			return CROSSAUDIO_EC_GENERIC;
		}
	}

	return CROSSAUDIO_EC_OK;
}

ErrorCode Engine::stop() {
	m_fd.close();

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
	oss_sysinfo sysInfo;
	if (!getSysInfo(sysInfo, m_fd)) {
		return nullptr;
	}

	std::map< std::string, Direction > nodeMap;

	for (decltype(sysInfo.numaudios) i = 0; i < sysInfo.numaudios; ++i) {
		oss_audioinfo info{};
		info.dev = i;
		if (!getAudioInfo(info, m_fd) || info.caps & PCM_CAP_HIDDEN) {
			continue;
		}

		std::string id = info.devnode;
		fixNodeID(id);

		auto &direction = nodeMap[id];

		auto dir = static_cast< uint8_t >(direction);
		if (info.caps & PCM_CAP_INPUT) {
			dir |= CROSSAUDIO_DIR_IN;
		}
		if (info.caps & PCM_CAP_OUTPUT) {
			dir |= CROSSAUDIO_DIR_OUT;
		}

		direction = static_cast< Direction >(dir);
	}

	auto nodes = nodesNew(nodeMap.size());

	for (auto [i, iter] = std::tuple{ std::size_t(0), nodeMap.cbegin() }; iter != nodeMap.cend(); ++i, ++iter) {
		nodes->items[i].id        = strdup(iter->first.data());
		nodes->items[i].name      = strdup(strrchr(iter->first.data(), '/') + 1);
		nodes->items[i].direction = iter->second;
	}

	return nodes;
}

// https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=246231
void Engine::fixNodeID(std::string &str) {
	const auto dotPos = str.find_last_of('.');
	if (dotPos == str.npos) {
		return;
	}

	const auto slashPos = str.find_last_of('/');
	if (slashPos != str.npos && dotPos < slashPos) {
		return;
	}

	// We didn't return, meaning this is not a valid ID/path. Example:
	//
	// - /dev/dsp0.p0
	// - /dev/dsp0.vp0
	// - /dev/dsp0.r0
	// - /dev/dsp0.vr0
	//
	// We need to remove everything from the dot onward.
	str.resize(dotPos);
}

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
