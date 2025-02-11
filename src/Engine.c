// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Engine.h"

#include "Backend.h"

#include <stdlib.h>

typedef struct CrossAudio_Nodes Nodes;

Engine *CrossAudio_engineNew(const Backend backend) {
	const BE_Impl *impl = backendGetImpl(backend);
	if (!impl) {
		return NULL;
	}

	void *data = impl->engineNew();
	if (!data) {
		return NULL;
	}

	Engine *engine = malloc(sizeof(*engine));

	engine->beImpl = impl;
	engine->beData = data;

	return engine;
}

ErrorCode CrossAudio_engineFree(Engine *engine) {
	const ErrorCode ec = engine->beImpl->engineFree(engine->beData);
	if (ec == CROSSAUDIO_EC_OK) {
		free(engine);
	}

	return ec;
}

ErrorCode CrossAudio_engineStart(Engine *engine, const EngineFeedback *feedback) {
	return engine->beImpl->engineStart(engine->beData, feedback);
}

ErrorCode CrossAudio_engineStop(Engine *engine) {
	return engine->beImpl->engineStop(engine->beData);
}

const char *CrossAudio_engineNameGet(Engine *engine) {
	return engine->beImpl->engineNameGet(engine->beData);
}

ErrorCode CrossAudio_engineNameSet(Engine *engine, const char *name) {
	return engine->beImpl->engineNameSet(engine->beData, name);
}

Nodes *CrossAudio_engineNodesGet(Engine *engine) {
	return engine->beImpl->engineNodesGet(engine->beData);
}
