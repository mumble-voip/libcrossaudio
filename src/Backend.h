// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKEND_H
#define CROSSAUDIO_SRC_BACKEND_H

#include "crossaudio/Backend.h"

#include "crossaudio/Direction.h"

#include <stddef.h>

typedef enum CrossAudio_Backend Backend;
typedef enum CrossAudio_ErrorCode ErrorCode;

typedef struct CrossAudio_FluxConfig FluxConfig;
typedef struct CrossAudio_FluxFeedback FluxFeedback;
typedef struct CrossAudio_Node Node;

typedef struct BE_Engine BE_Engine;
typedef struct BE_Flux BE_Flux;

typedef struct BE_Impl {
	const char *(*name)();
	const char *(*version)();

	ErrorCode (*init)();
	ErrorCode (*deinit)();

	BE_Engine *(*engineNew)();
	ErrorCode (*engineFree)(BE_Engine *engine);
	ErrorCode (*engineStart)(BE_Engine *engine);
	ErrorCode (*engineStop)(BE_Engine *engine);
	const char *(*engineNameGet)(BE_Engine *engine);
	ErrorCode (*engineNameSet)(BE_Engine *engine, const char *name);
	Node *(*engineNodesGet)(BE_Engine *engine);
	ErrorCode (*engineNodesFree)(BE_Engine *engine, Node *nodes);

	BE_Flux *(*fluxNew)(BE_Engine *engine);
	ErrorCode (*fluxFree)(BE_Flux *flux);
	ErrorCode (*fluxStart)(BE_Flux *flux, FluxConfig *config, const FluxFeedback *feedback);
	ErrorCode (*fluxStop)(BE_Flux *flux);
	const char *(*fluxNameGet)(BE_Flux *flux);
	ErrorCode (*fluxNameSet)(BE_Flux *flux, const char *name);
} BE_Impl;

static inline const BE_Impl *backendGetImpl(const Backend backend) {
	switch (backend) {
		case CROSSAUDIO_BACKEND_DUMMY: {
			// extern Backend_Impl Dummy_Impl;
			// return &Dummy_Impl;
		}
		case CROSSAUDIO_BACKEND_ALSA: {
			// extern Backend_Impl ALSA_Impl;
			// return &ALSA_Impl;
		}
		case CROSSAUDIO_BACKEND_OSS: {
			// extern Backend_Impl OSS_Impl;
			// return &OSS_Impl;
		}
		case CROSSAUDIO_BACKEND_WASAPI: {
#ifdef HAS_WASAPI
			extern const BE_Impl WASAPI_Impl;
			return &WASAPI_Impl;
#endif
		}
		case CROSSAUDIO_BACKEND_COREAUDIO: {
			// extern Backend_Impl CoreAudio_Impl;
			// return &CoreAudio_Impl;
		}
		case CROSSAUDIO_BACKEND_PULSEAUDIO: {
			// extern Backend_Impl PulseAudio_Impl;
			// return &PulseAudio_Impl;
		}
		case CROSSAUDIO_BACKEND_JACKAUDIO: {
			// extern Backend_Impl JackAudio_Impl;
			// return &JackAudio_Impl;
		}
		case CROSSAUDIO_BACKEND_PIPEWIRE: {
#ifdef HAS_PIPEWIRE
			extern const BE_Impl PipeWire_Impl;
			return &PipeWire_Impl;
#endif
		}
	}

	return NULL;
}

#endif
