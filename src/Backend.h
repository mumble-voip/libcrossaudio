// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_SRC_BACKEND_H
#define CROSSAUDIO_SRC_BACKEND_H

#include "crossaudio/Backend.h"

#include "crossaudio/Direction.h"

#include <stddef.h>

#define GET_BACKEND(name)             \
	extern const BE_Impl name##_Impl; \
	return &name##_Impl;

typedef enum CrossAudio_Backend Backend;
typedef enum CrossAudio_ErrorCode ErrorCode;

typedef struct CrossAudio_EngineFeedback EngineFeedback;
typedef struct CrossAudio_FluxConfig FluxConfig;
typedef struct CrossAudio_FluxFeedback FluxFeedback;
typedef struct CrossAudio_Nodes Nodes;

typedef struct BE_Engine BE_Engine;
typedef struct BE_Flux BE_Flux;

typedef struct BE_Impl {
	const char *(*name)(void);
	const char *(*version)(void);

	ErrorCode (*init)(void);
	ErrorCode (*deinit)(void);

	BE_Engine *(*engineNew)(void);
	ErrorCode (*engineFree)(BE_Engine *engine);
	ErrorCode (*engineStart)(BE_Engine *engine, const EngineFeedback *feedback);
	ErrorCode (*engineStop)(BE_Engine *engine);
	const char *(*engineNameGet)(BE_Engine *engine);
	ErrorCode (*engineNameSet)(BE_Engine *engine, const char *name);
	Nodes *(*engineNodesGet)(BE_Engine *engine);

	BE_Flux *(*fluxNew)(BE_Engine *engine);
	ErrorCode (*fluxFree)(BE_Flux *flux);
	ErrorCode (*fluxStart)(BE_Flux *flux, FluxConfig *config, const FluxFeedback *feedback);
	ErrorCode (*fluxStop)(BE_Flux *flux);
	ErrorCode (*fluxPause)(BE_Flux *flux, bool on);
	const char *(*fluxNameGet)(BE_Flux *flux);
	ErrorCode (*fluxNameSet)(BE_Flux *flux, const char *name);
} BE_Impl;

static inline const BE_Impl *backendGetImpl(const Backend backend) {
	switch (backend) {
		case CROSSAUDIO_BACKEND_DUMMY: {
#ifdef HAS_DUMMY
			GET_BACKEND(Dummy)
#else
			return NULL;
#endif
		}
		case CROSSAUDIO_BACKEND_ALSA: {
#ifdef HAS_ALSA
			GET_BACKEND(ALSA)
#else
			return NULL;
#endif
		}
		case CROSSAUDIO_BACKEND_OSS: {
#ifdef HAS_OSS
			GET_BACKEND(OSS)
#else
			return NULL;
#endif
		}
		case CROSSAUDIO_BACKEND_WASAPI: {
#ifdef HAS_WASAPI
			GET_BACKEND(WASAPI)
#else
			return NULL;
#endif
		}
		case CROSSAUDIO_BACKEND_COREAUDIO: {
#ifdef HAS_COREAUDIO
			GET_BACKEND(CoreAudio)
#else
			return NULL;
#endif
		}
		case CROSSAUDIO_BACKEND_PULSEAUDIO: {
#ifdef HAS_PULSEAUDIO
			GET_BACKEND(PulseAudio)
#else
			return NULL;
#endif
		}
		case CROSSAUDIO_BACKEND_SNDIO: {
#ifdef HAS_SNDIO
			GET_BACKEND(Sndio)
#else
			return NULL;
#endif
		}
		case CROSSAUDIO_BACKEND_PIPEWIRE: {
#ifdef HAS_PIPEWIRE
			GET_BACKEND(PipeWire)
#else
			return NULL;
#endif
		}
	}

	return NULL;
}

#endif
