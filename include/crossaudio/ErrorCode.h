// Copyright 2022-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef CROSSAUDIO_ERRORCODE_H
#define CROSSAUDIO_ERRORCODE_H

enum CrossAudio_ErrorCode {
	CROSSAUDIO_EC_GENERIC = -1,
	CROSSAUDIO_EC_OK,
	CROSSAUDIO_EC_NULL,
	CROSSAUDIO_EC_INIT,
	CROSSAUDIO_EC_BUSY,
	CROSSAUDIO_EC_LIBRARY,
	CROSSAUDIO_EC_SYMBOL,
	CROSSAUDIO_EC_CONNECT
};

static inline const char *CrossAudio_ErrorCodeText(const enum CrossAudio_ErrorCode ec) {
	switch (ec) {
		case CROSSAUDIO_EC_GENERIC:
			return "CROSSAUDIO_EC_GENERIC";
		case CROSSAUDIO_EC_OK:
			return "CROSSAUDIO_EC_OK";
		case CROSSAUDIO_EC_NULL:
			return "CROSSAUDIO_EC_NULL";
		case CROSSAUDIO_EC_INIT:
			return "CROSSAUDIO_EC_INIT";
		case CROSSAUDIO_EC_BUSY:
			return "CROSSAUDIO_EC_BUSY";
		case CROSSAUDIO_EC_LIBRARY:
			return "CROSSAUDIO_EC_LIBRARY";
		case CROSSAUDIO_EC_SYMBOL:
			return "CROSSAUDIO_EC_SYMBOL";
		case CROSSAUDIO_EC_CONNECT:
			return "CROSSAUDIO_EC_CONNECT";
	}

	return "";
}

#endif
