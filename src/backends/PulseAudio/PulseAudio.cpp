// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "PulseAudio.hpp"

#include "Engine.hpp"
#include "Flux.hpp"
#include "Library.hpp"

#include "Backend.h"

#include "crossaudio/ErrorCode.h"

#include <string_view>

using namespace pulseaudio;

static auto toImpl(BE_Engine *engine) {
	return reinterpret_cast< Engine * >(engine);
}

static auto toImpl(BE_Flux *flux) {
	return reinterpret_cast< Flux * >(flux);
}

static const char *name() {
	return "PulseAudio";
}

static const char *version() {
	return lib().get_library_version();
}

static ErrorCode init() {
	// clang-format off
	constexpr std::string_view names[] = {
		"libpulse.so",
		"libpulse.so.0"
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
	if (auto engine = new Engine()) {
		if (*engine) {
			return reinterpret_cast< BE_Engine * >(engine);
		}

		delete engine;
	}

	return nullptr;
}

static ErrorCode engineFree(BE_Engine *engine) {
	delete toImpl(engine);

	return CROSSAUDIO_EC_OK;
}

static ErrorCode engineStart(BE_Engine *engine, const EngineFeedback *feedback) {
	return toImpl(engine)->start(feedback ? *feedback : EngineFeedback());
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

static BE_Flux *fluxNew(BE_Engine *engine) {
	return reinterpret_cast< BE_Flux * >(new Flux(*toImpl(engine)));
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
constexpr BE_Impl PulseAudio_Impl = {
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
