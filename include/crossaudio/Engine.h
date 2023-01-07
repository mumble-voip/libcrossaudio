// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_ENGINE_H
#define CROSSAUDIO_ENGINE_H

#include "Backend.h"

struct CrossAudio_Engine;

CROSSAUDIO_EXPORT struct CrossAudio_Engine *CrossAudio_engineNew(enum CrossAudio_Backend backend);
CROSSAUDIO_EXPORT enum CrossAudio_ErrorCode CrossAudio_engineFree(struct CrossAudio_Engine *engine);

CROSSAUDIO_EXPORT enum CrossAudio_ErrorCode CrossAudio_engineStart(struct CrossAudio_Engine *engine);
CROSSAUDIO_EXPORT enum CrossAudio_ErrorCode CrossAudio_engineStop(struct CrossAudio_Engine *engine);

CROSSAUDIO_EXPORT const char *CrossAudio_engineNameGet(struct CrossAudio_Engine *engine);
CROSSAUDIO_EXPORT enum CrossAudio_ErrorCode CrossAudio_engineNameSet(struct CrossAudio_Engine *engine,
																	 const char *name);

#endif
