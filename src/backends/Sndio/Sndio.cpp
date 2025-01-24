// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Sndio.hpp"

#include "Engine.hpp"
#include "Flux.hpp"
#include "Library.hpp"

#include "Backend.h"

#include "crossaudio/ErrorCode.h"

#include <string_view>

using namespace sndio;

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
