// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_ENGINE_H
#define CROSSAUDIO_ENGINE_H

#include "Backend.h"

struct CrossAudio_Engine;
struct CrossAudio_Node;

struct CrossAudio_EngineFeedback {
	void *userData;

	void (*nodeAdded)(void *userData, struct CrossAudio_Node *node);
	void (*nodeRemoved)(void *userData, struct CrossAudio_Node *node);
};

#ifdef __cplusplus
extern "C" {
#endif

CROSSAUDIO_EXPORT struct CrossAudio_Engine *CrossAudio_engineNew(enum CrossAudio_Backend backend);
CROSSAUDIO_EXPORT enum CrossAudio_ErrorCode CrossAudio_engineFree(struct CrossAudio_Engine *engine);

CROSSAUDIO_EXPORT enum CrossAudio_ErrorCode CrossAudio_engineStart(struct CrossAudio_Engine *engine,
																   const struct CrossAudio_EngineFeedback *feedback);
CROSSAUDIO_EXPORT enum CrossAudio_ErrorCode CrossAudio_engineStop(struct CrossAudio_Engine *engine);

CROSSAUDIO_EXPORT const char *CrossAudio_engineNameGet(struct CrossAudio_Engine *engine);
CROSSAUDIO_EXPORT enum CrossAudio_ErrorCode CrossAudio_engineNameSet(struct CrossAudio_Engine *engine,
																	 const char *name);

CROSSAUDIO_EXPORT struct CrossAudio_Nodes *CrossAudio_engineNodesGet(struct CrossAudio_Engine *engine);

#ifdef __cplusplus
}
#endif

#endif
