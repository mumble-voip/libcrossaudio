// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "OSS.hpp"

#include "Engine.hpp"
#include "Flux.hpp"
#include "Mixer.hpp"

#include "Backend.h"

#include "crossaudio/ErrorCode.h"

#include <cstdio>
#include <cstring>

#include <sys/soundcard.h>

using namespace oss;

static auto toImpl(BE_Engine *engine) {
	return reinterpret_cast< Engine * >(engine);
}

static auto toImpl(BE_Flux *flux) {
	return reinterpret_cast< Flux * >(flux);
}

static const char *name() {
	return "OSS";
}

static const char *version() {
	Mixer mixer;
	if (!mixer.open()) {
		return nullptr;
	}

	oss_sysinfo info;
	if (!mixer.getSysInfo(info)) {
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
