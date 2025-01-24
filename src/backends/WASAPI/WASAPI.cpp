// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "WASAPI.hpp"

#include "Engine.hpp"
#include "Flux.hpp"

#include "Backend.h"

#include <vector>

#include <Windows.h>

using namespace wasapi;

static auto toImpl(BE_Engine *engine) {
	return reinterpret_cast< Engine * >(engine);
}

static auto toImpl(BE_Flux *flux) {
	return reinterpret_cast< Flux * >(flux);
}

static const char *name() {
	return "WASAPI";
}

static const char *version() {
	constexpr auto moduleName = "MMDevAPI.dll";

	DWORD handle;
	const auto infoSize = GetFileVersionInfoSize(moduleName, &handle);
	if (!infoSize) {
		return nullptr;
	}

	std::vector< std::byte > infoBuffer(infoSize);
	if (!GetFileVersionInfo(moduleName, handle, infoSize, infoBuffer.data())) {
		return nullptr;
	}

	void *versionBuffer;
	UINT versionSize;
	if (!VerQueryValue(infoBuffer.data(), "\\", &versionBuffer, &versionSize) || !versionSize) {
		return nullptr;
	}

	const auto version = static_cast< VS_FIXEDFILEINFO * >(versionBuffer);
	if (version->dwSignature != 0xfeef04bd) {
		return nullptr;
	}

	const auto major = static_cast< uint16_t >((version->dwFileVersionMS >> 16) & 0xffff);
	const auto minor = static_cast< uint16_t >((version->dwFileVersionMS >> 0) & 0xffff);
	const auto build = static_cast< uint16_t >((version->dwFileVersionLS >> 16) & 0xffff);
	const auto tweak = static_cast< uint16_t >((version->dwFileVersionLS >> 0) & 0xffff);

	// Max: "65535.65535.65535.65535"
	static char versionString[24];
	sprintf(versionString, "%hu.%hu.%hu.%hu", major, minor, build, tweak);

	return versionString;
}

static ErrorCode init() {
	return Engine::threadInit();
}

static ErrorCode deinit() {
	return Engine::threadDeinit();
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
	if (auto flux = new Flux(*toImpl(engine))) {
		if (*flux) {
			return reinterpret_cast< BE_Flux * >(flux);
		}

		delete flux;
	}

	return nullptr;
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
const BE_Impl WASAPI_Impl = {
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
