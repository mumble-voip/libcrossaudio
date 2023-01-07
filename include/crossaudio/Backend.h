// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_BACKEND_H
#define CROSSAUDIO_BACKEND_H

#include "ErrorCode.h"
#include "Macros.h"

#include <stdbool.h>

enum CrossAudio_Backend {
	CROSSAUDIO_BACKEND_DUMMY,
	CROSSAUDIO_BACKEND_ALSA,
	CROSSAUDIO_BACKEND_OSS,
	CROSSAUDIO_BACKEND_WASAPI,
	CROSSAUDIO_BACKEND_COREAUDIO,
	CROSSAUDIO_BACKEND_PULSEAUDIO,
	CROSSAUDIO_BACKEND_JACKAUDIO,
	CROSSAUDIO_BACKEND_PIPEWIRE
};

CROSSAUDIO_EXPORT bool CrossAudio_backendExists(enum CrossAudio_Backend backend);

CROSSAUDIO_EXPORT const char *CrossAudio_backendName(enum CrossAudio_Backend backend);
CROSSAUDIO_EXPORT const char *CrossAudio_backendVersion(enum CrossAudio_Backend backend);

CROSSAUDIO_EXPORT enum CrossAudio_ErrorCode CrossAudio_backendInit(enum CrossAudio_Backend backend);
CROSSAUDIO_EXPORT enum CrossAudio_ErrorCode CrossAudio_backendDeinit(enum CrossAudio_Backend backend);

#endif
