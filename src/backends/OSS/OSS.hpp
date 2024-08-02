// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKENDS_OSS_OSS_HPP
#define CROSSAUDIO_SRC_BACKENDS_OSS_OSS_HPP

#include "FileDescriptor.hpp"

#include "crossaudio/ErrorCode.h"
#include "crossaudio/Flux.h"
#include "crossaudio/Node.h"

#include <atomic>
#include <memory>
#include <string>

typedef CrossAudio_FluxConfig FluxConfig;
typedef CrossAudio_FluxFeedback FluxFeedback;

typedef CrossAudio_ErrorCode ErrorCode;
typedef CrossAudio_Nodes Nodes;

struct BE_Impl;

#ifdef __linux__
namespace std {
class thread;
}
#endif

namespace oss {
class Engine {
public:
	Engine();
	~Engine();

	constexpr operator bool() const { return static_cast< bool >(m_fd); }

	const char *nameGet() const;
	ErrorCode nameSet(const char *name);

	Nodes *engineNodesGet();

	ErrorCode start();
	ErrorCode stop();

private:
	Engine(const Engine &)            = delete;
	Engine &operator=(const Engine &) = delete;

	static void fixNodeID(std::string &str);

	FileDescriptor m_fd;
	std::string m_name;
};

class Flux {
public:
	Flux();
	~Flux();

	constexpr operator bool() const { return static_cast< bool >(m_fd); }

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

	static constexpr int translateFormat(CrossAudio_BitFormat format, uint8_t sampleBits);

	FluxConfig m_config;
	FluxFeedback m_feedback;

	FileDescriptor m_fd;

	std::atomic_bool m_halt;
	std::atomic_flag m_pause;
	std::unique_ptr< std::thread > m_thread;
};
} // namespace oss

// Backend API

extern const BE_Impl OSS_Impl;

#endif
