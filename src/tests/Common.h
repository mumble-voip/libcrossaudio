// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_TESTS_COMMON_H
#define CROSSAUDIO_TESTS_COMMON_H

#include "crossaudio/Engine.h"
#include "crossaudio/Flux.h"

#include <stdbool.h>
#include <stdio.h>

#if defined(OS_LINUX)
#	define BACKEND CROSSAUDIO_BACKEND_PIPEWIRE
#elif defined(OS_WINDOWS)
#	define BACKEND CROSSAUDIO_BACKEND_WASAPI
#endif

typedef enum CrossAudio_Backend Backend;
typedef enum CrossAudio_ErrorCode ErrorCode;

typedef struct CrossAudio_Engine Engine;
typedef struct CrossAudio_Flux Flux;
typedef struct CrossAudio_FluxConfig FluxConfig;
typedef struct CrossAudio_FluxFeedback FluxFeedback;

static inline bool initBackend() {
	const ErrorCode ec = CrossAudio_backendInit(BACKEND);
	if (ec != CROSSAUDIO_EC_OK) {
		printf("CrossAudio_backendInit() failed with error \"%s\"!\n", CrossAudio_ErrorCodeText(ec));
		return false;
	}

	printf("Backend name: %s | version: %s\n\n", CrossAudio_backendName(BACKEND), CrossAudio_backendVersion(BACKEND));

	return true;
}

static inline bool deinitBackend() {
	const ErrorCode ec = CrossAudio_backendDeinit(BACKEND);
	if (ec != CROSSAUDIO_EC_OK) {
		printf("CrossAudio_backendDeinit() failed with error \"%s\"!\n", CrossAudio_ErrorCodeText(ec));
		return false;
	}

	return true;
}

static inline Engine *createEngine() {
	Engine *engine = CrossAudio_engineNew(BACKEND);
	if (!engine) {
		printf("CrossAudio_engineNew() failed!\n");
		return NULL;
	}

	const ErrorCode ec = CrossAudio_engineStart(engine);
	if (ec != CROSSAUDIO_EC_OK) {
		printf("CrossAudio_engineStart() failed with error \"%s\"!\n", CrossAudio_ErrorCodeText(ec));
		CrossAudio_engineFree(engine);
		return NULL;
	}

	return engine;
}

static inline bool destroyEngine(Engine *engine) {
	if (!engine) {
		return true;
	}

	const ErrorCode ec = CrossAudio_engineFree(engine);
	if (ec != CROSSAUDIO_EC_OK) {
		printf("CrossAudio_engineFree() failed with error \"%s\"!\n", CrossAudio_ErrorCodeText(ec));
		return false;
	}

	return true;
}

static inline Flux *createStream(Engine *engine, FluxConfig *config, const FluxFeedback *feedback) {
	Flux *flux = CrossAudio_fluxNew(engine);
	if (!flux) {
		printf("CrossAudio_fluxNew() failed!\n");
		return NULL;
	}

	const ErrorCode ec = CrossAudio_fluxStart(flux, config, feedback);
	if (ec != CROSSAUDIO_EC_OK) {
		printf("CrossAudio_fluxStart() failed with error \"%s\"!\n", CrossAudio_ErrorCodeText(ec));
		CrossAudio_fluxFree(flux);
		return NULL;
	}

	return flux;
}

static inline bool destroyStream(Flux *flux) {
	if (!flux) {
		return true;
	}

	const ErrorCode ec = CrossAudio_fluxFree(flux);
	if (ec != CROSSAUDIO_EC_OK) {
		printf("CrossAudio_fluxFree() failed with error \"%s\"!\n", CrossAudio_ErrorCodeText(ec));
		return false;
	}

	return true;
}

#endif
