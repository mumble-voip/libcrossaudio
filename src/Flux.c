// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Flux.h"

#include "Backend.h"
#include "Engine.h"

#include <stdlib.h>

Flux *CrossAudio_fluxNew(Engine *engine) {
	const BE_Impl *impl = engine->beImpl;

	void *data = impl->fluxNew(engine->beData);
	if (!data) {
		return NULL;
	}

	Flux *flux = malloc(sizeof(*flux));

	flux->beImpl = impl;
	flux->beData = data;
	flux->engine = engine;

	return flux;
}

ErrorCode CrossAudio_fluxFree(Flux *flux) {
	const ErrorCode ec = flux->beImpl->fluxFree(flux->beData);
	if (ec == CROSSAUDIO_EC_OK) {
		free(flux);
	}

	return ec;
}

Engine *CrossAudio_fluxEngine(Flux *flux) {
	return flux->engine;
}

ErrorCode CrossAudio_fluxStart(Flux *flux, FluxConfig *config, const FluxFeedback *feedback) {
	return flux->beImpl->fluxStart(flux->beData, config, feedback);
}

ErrorCode CrossAudio_fluxStop(Flux *flux) {
	return flux->beImpl->fluxStop(flux->beData);
}

const char *CrossAudio_fluxNameGet(Flux *flux) {
	return flux->beImpl->fluxNameGet(flux->beData);
}

ErrorCode CrossAudio_fluxNameSet(Flux *flux, const char *name) {
	return flux->beImpl->fluxNameSet(flux->beData, name);
}
