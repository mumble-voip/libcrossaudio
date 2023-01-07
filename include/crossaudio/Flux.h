// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_FLUX_H
#define CROSSAUDIO_FLUX_H

#include "Channel.h"
#include "Direction.h"
#include "ErrorCode.h"
#include "Macros.h"

#include <stdint.h>

struct CrossAudio_Engine;
struct CrossAudio_Flux;

struct CrossAudio_FluxConfig {
	enum CrossAudio_Direction direction;
	uint8_t channels;
	uint32_t position[CROSSAUDIO_CH_NUM];
	uint32_t sampleRate;
};

struct CrossAudio_FluxData {
	void *data;
	uint32_t frames;
};

struct CrossAudio_FluxFeedback {
	void *userData;

	void (*process)(void *userData, struct CrossAudio_FluxData *data);
};

CROSSAUDIO_EXPORT struct CrossAudio_Flux *CrossAudio_fluxNew(struct CrossAudio_Engine *engine);
CROSSAUDIO_EXPORT enum CrossAudio_ErrorCode CrossAudio_fluxFree(struct CrossAudio_Flux *flux);

CROSSAUDIO_EXPORT struct CrossAudio_Engine *CrossAudio_fluxEngine(struct CrossAudio_Flux *flux);

CROSSAUDIO_EXPORT enum CrossAudio_ErrorCode CrossAudio_fluxStart(struct CrossAudio_Flux *flux,
																 struct CrossAudio_FluxConfig *config,
																 const struct CrossAudio_FluxFeedback *feedback);
CROSSAUDIO_EXPORT enum CrossAudio_ErrorCode CrossAudio_fluxStop(struct CrossAudio_Flux *flux);

CROSSAUDIO_EXPORT const char *CrossAudio_fluxNameGet(struct CrossAudio_Flux *flux);
CROSSAUDIO_EXPORT enum CrossAudio_ErrorCode CrossAudio_fluxNameSet(struct CrossAudio_Flux *flux, const char *name);

#endif
