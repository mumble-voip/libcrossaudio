// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Backend.h"

bool CrossAudio_backendExists(const Backend backend) {
	return backendGetImpl(backend) != NULL;
}

const char *CrossAudio_backendName(const Backend backend) {
	const BE_Impl *impl = backendGetImpl(backend);

	return impl ? impl->name() : NULL;
}

const char *CrossAudio_backendVersion(const Backend backend) {
	const BE_Impl *impl = backendGetImpl(backend);

	return impl ? impl->version() : NULL;
}

ErrorCode CrossAudio_backendInit(const Backend backend) {
	const BE_Impl *impl = backendGetImpl(backend);

	return impl ? impl->init() : CROSSAUDIO_EC_NULL;
}

ErrorCode CrossAudio_backendDeinit(const Backend backend) {
	const BE_Impl *impl = backendGetImpl(backend);

	return impl ? impl->deinit() : CROSSAUDIO_EC_NULL;
}
